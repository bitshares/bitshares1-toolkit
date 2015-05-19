#include <bts/witness/witness.hpp>

#include <bts/chain/witness_object.hpp>
#include <bts/time/time.hpp>

#include <fc/thread/thread.hpp>

using namespace bts::witness_plugin;
using std::string;
using std::vector;

void witness_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   command_line_options.add_options()
         ("enable-stale-production", bpo::bool_switch()->notifier([this](bool e){_production_enabled = e;}), "Enable block production, even if the chain is stale")
         ("witness-id,w", bpo::value<vector<string>>()->composing()->multitoken(),
          "ID of witness controlled by this node (e.g. \"1.7.0\", quotes are required, may specify multiple times)")
         ("private-key", bpo::value<vector<string>>()->composing()->multitoken()->
          DEFAULT_VALUE_VECTOR(std::make_pair(chain::key_id_type(), fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("genesis"))))),
          "Tuple of [key ID, private key] (may specify multiple times)")
         ;
   config_file_options.add(command_line_options);
}

std::string witness_plugin::plugin_name()const
{
   return "witness";
}

void witness_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   _options = &options;
   LOAD_VALUE_SET(options, "witness-id", _witnesses, chain::witness_id_type)
   //Define a type T which doesn't have a comma, as I can't put a comma in a macro argument
   using T = std::pair<chain::key_id_type,fc::ecc::private_key>;
   LOAD_VALUE_SET(options, "private-key", _private_keys, T)
}

void witness_plugin::plugin_startup()
{ try {
      std::set<chain::witness_id_type> bad_wits;
      //Start NTP time client
      bts::time::now();
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
      app().set_block_production(true);
      schedule_next_production(database().get_global_properties().parameters);
   } else
      elog("No witnesses configured! Please add witness IDs and private keys to configuration.");
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::plugin_shutdown()
{
   bts::time::shutdown_ntp_time();
   return;
}

void witness_plugin::schedule_next_production(const bts::chain::chain_parameters& global_parameters)
{
   //Get next production time for *any* delegate
   auto block_interval = global_parameters.block_interval;
   fc::time_point next_block_time = fc::time_point_sec() +
         (bts::time::now().sec_since_epoch() / block_interval + 1) * block_interval;

   if( bts::time::ntp_time().valid() )
      next_block_time -= bts::time::ntp_error();

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
       llabs((db.head_block_time() - bts::time::now()).to_seconds()) <= global_parameters.block_interval )
      _production_enabled = true;

   // is anyone scheduled to produce now or one second in the future?
   fc::optional< std::pair< fc::time_point_sec, bts::chain::witness_id_type > > sch = db.get_scheduled_witness( bts::time::now() + fc::seconds(1) );
   fc::time_point_sec now = bts::time::now();

   auto is_scheduled = [&]()
   {
      // conditions needed to produce a block:

      // block production must be enabled (i.e. witness must be synced)
      if( !_production_enabled )
         return false;

      if( !sch.valid() )
         return false;

      // the next block must be scheduled after the head block.
      // if this check fails, the local clock has not advanced far
      // enough from the head block for the DB to report a witness.
      if( sch->first <= db.head_block_time() )
         return false;

      // we must control the witness scheduled to produce the next block.
      if( _witnesses.find( sch->second ) == _witnesses.end() )
         return false;

      // the current clock must be at least 1 second ahead of
      // head_block_time.
      if( (now - db.head_block_time()).to_seconds() < 1 )
         return false;

      // the current clock must be within 500 milliseconds of
      // the scheduled production time.
      if( llabs((sch->first - now).count()) > fc::milliseconds(500).count() )
         return false;

      return true;
   };

   wdump((sch)(now));
   if( is_scheduled() )
   {
      ilog("Witness ${id} production slot has arrived; generating a block now...", ("id", sch->second));
      try
      {
         auto block = db.generate_block(
            sch->first,
            sch->second,
            _private_keys[ sch->second( db ).signing_key ]
            );
         ilog("Generated block #${n} with timestamp ${t} at time ${c}",
              ("n", block.block_num())("t", block.timestamp)("c", now));
         p2p_node().broadcast(net::block_message(block));
      }
      catch( const fc::canceled_exception& )
      {
         //We're trying to exit. Go ahead and let this one out.
         throw;
      }
      catch( const fc::exception& e )
      {
         elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
      }
   }

   schedule_next_production(global_parameters);
}
