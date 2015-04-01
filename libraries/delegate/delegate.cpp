#include <bts/delegate/delegate.hpp>

#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>

void bts::delegate_plugin::delegate_plugin::configure(const bts::delegate_plugin::delegate_plugin::plugin_config& cfg)
{
   _config = cfg;

   if( !_config.delegate_keys.empty() )
      _block_production_task = fc::async([this]{block_production_loop();}, "Delegate Block Production");
   else
      elog("No delegates configured! Please add delegate IDs and private keys to configuration.");
}

void bts::delegate_plugin::delegate_plugin::block_production_loop()
{
   chain::database& db = database();
   std::set<chain::delegate_id_type> delegates;

   std::transform(_config.delegate_keys.begin(), _config.delegate_keys.end(),
                  std::inserter(delegates, delegates.begin()),
                  [](const std::pair<chain::delegate_id_type,fc::ecc::private_key>& pair) {
                     return pair.first;
                  });

   auto next_production = db.get_next_generation_time(delegates);
   wdump((next_production)(chain::now()));
   if( llabs((next_production.first - chain::now()).count()) <= fc::seconds(1).count() || db.head_block_num() == 0 )
   {
      ilog("Delegate ${id} production slot has arrived; generating a block now...", ("id", next_production.second));
      try {
         auto block = db.generate_block(_config.delegate_keys[next_production.second], next_production.second);
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

   //Get next production time for *any* delegate
   auto block_interval = db.get_global_properties().block_interval;
   fc::time_point_sec next_block_time = db.head_block_time() + block_interval;
   if(db.head_block_num() == 0)
      next_block_time = chain::now();

   //Sleep until the next production time for *any* delegate
   fc::microseconds sleep_duration = std::max(next_block_time - chain::now(), fc::seconds(0));
   ilog("Delegate is sleeping until next block production time in ${d} seconds.", ("d", sleep_duration.to_seconds()));
   _block_production_task = fc::schedule([this]{block_production_loop();},
                                         fc::time_point::now() + sleep_duration, "Delegate Block Production");
}
