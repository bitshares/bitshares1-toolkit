#include <bts/witness/witness.hpp>

#include <bts/chain/time.hpp>
#include <bts/chain/witness_object.hpp>

#include <fc/thread/thread.hpp>

using namespace bts::witness_plugin;
using std::string;
using std::vector;

void witness_plugin::set_program_options_impl(boost::program_options::options_description& command_line_options,
                                              boost::program_options::options_description& config_file_options)
{
   command_line_options.add_options()
         ("enable-stale-production", bpo::bool_switch()->notifier([this](bool e){_production_enabled = e;}), "Enable block production, even if the chain is stale")
         ("witness-id,w", bpo::value<vector<string>>()->composing()->multitoken(),
          "ID of witness controlled by this node (e.g. \"1.7.0\", may specify multiple times)")
         ("private-key", bpo::value<vector<string>>()->composing()->multitoken()->
          DEFAULT_VALUE_VECTOR(std::make_pair(chain::key_id_type(), fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("genesis"))))),
          "Tuple of [key ID, private key] (may specify multiple times)")
         ;
   config_file_options.add(command_line_options);
}

void witness_plugin::initialize(const boost::program_options::variables_map& options)
{
   _options = &options;
   LOAD_VALUE_SET(options, "witness-id", _witnesses, chain::witness_id_type)
   //Define a type T which doesn't have a comma, as I can't put a comma in a macro argument
   using T = std::pair<chain::key_id_type,fc::ecc::private_key>;
   LOAD_VALUE_SET(options, "private-key", _private_keys, T)
}

void witness_plugin::startup()
{ try {
      std::set<chain::witness_id_type> bad_wits;
      for( auto wit : _witnesses )
   {
      auto key = wit(database()).signing_key;
      if( !_private_keys.count(key) )
      {
         elog("Unable to find key for witness ${w}. Removing it from my witnesses.", ("w", wit));
         bad_wits.insert(wit);
      }
   }
   for( auto wit : bad_wits )
      _witnesses.erase(wit);

   if( !_witnesses.empty() )
   {
      ilog("Launching block production for ${n} witnesses.", ("n", _witnesses.size()));
      schedule_next_production(database().get_global_properties().parameters);
   } else
      elog("No witnesses configured! Please add witness IDs and private keys to configuration.");
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::schedule_next_production(const bts::chain::chain_parameters& global_parameters)
{
   //Get next production time for *any* delegate
   auto block_interval = global_parameters.block_interval;
   fc::time_point next_block_time = fc::time_point_sec() +
         (chain::now().sec_since_epoch() / block_interval + 1) * block_interval;

   if( chain::ntp_time().valid() )
      next_block_time -= chain::ntp_error();

   //Sleep until the next production time for *any* delegate
   _block_production_task = fc::schedule([this]{block_production_loop();},
                                         next_block_time, "Witness Block Production");
}

void witness_plugin::block_production_loop()
{
   chain::database& db = database();
   const auto& global_parameters = db.get_global_properties().parameters;

   // Is there a head block within a block interval of now? If so, we're synced and can begin production.
   if( !_production_enabled &&
       llabs((db.head_block_time() - chain::now()).to_seconds()) <= global_parameters.block_interval )
      _production_enabled = true;

   auto next_production = db.get_next_generation_time(_witnesses);
   wdump((next_production)(chain::now()));
   if( _production_enabled &&
       (llabs((next_production.first - chain::now()).count()) <= fc::milliseconds(500).count()) &&
       (chain::now() - db.head_block_time()).to_seconds() >= 1 )
   {
      ilog("Witness ${id} production slot has arrived; generating a block now...", ("id", next_production.second));
      try {
         auto block = db.generate_block(_private_keys[next_production.second(database()).signing_key], next_production.second);
         ilog("Generated block #${n} with timestamp ${t} at time ${c}",
              ("n", block.block_num())("t", block.timestamp)("c", chain::now()));
         p2p_node().broadcast(net::block_message(block));
      } catch( const fc::canceled_exception& ) {
         //We're trying to exit. Go ahead and let this one out.
         throw;
      } catch( const fc::exception& e ) {
         elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
      }
   }

   schedule_next_production(global_parameters);
}
