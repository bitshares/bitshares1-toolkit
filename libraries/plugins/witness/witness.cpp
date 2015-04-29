#include <bts/witness/witness.hpp>

#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>

using namespace bts::witness_plugin;

void witness_plugin::configure(const bts::witness_plugin::witness_plugin::plugin_config& cfg)
{ try {
   _config = cfg;
} FC_CAPTURE_AND_RETHROW() }

void witness_plugin::init()
{ try {
   if( !_config.witness_keys.empty() )
      schedule_next_production(database().get_global_properties().parameters);
   else
      elog("No witnesses configured! Please add delegate IDs and private keys to configuration.");
   _production_enabled = _config.allow_production_on_stale_chain;
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
   std::set<chain::witness_id_type> witnesses;
   const auto& global_parameters = db.get_global_properties().parameters;

   // Is there a head block within a block interval of now? If so, we're synced and can begin production.
   if( !_production_enabled &&
       llabs((db.head_block_time() - chain::now()).to_seconds()) <= global_parameters.block_interval )
      _production_enabled = true;
   idump((_production_enabled));

   std::transform(_config.witness_keys.begin(), _config.witness_keys.end(),
                  std::inserter(witnesses, witnesses.begin()),
                  [](const std::pair<chain::witness_id_type,fc::ecc::private_key>& pair) {
                     return pair.first;
                  });

   auto next_production = db.get_next_generation_time(witnesses);
   wdump((next_production)(chain::now()));
   if( _production_enabled &&
       (llabs((next_production.first - chain::now()).count()) <= fc::seconds(1).count() || db.head_block_num() == 0) )
   {
      ilog("Witness ${id} production slot has arrived; generating a block now...", ("id", next_production.second));
      try {
         auto block = db.generate_block(_config.witness_keys[next_production.second], next_production.second);
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
