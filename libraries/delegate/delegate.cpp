#include <bts/delegate/delegate.hpp>

#include <fc/thread/thread.hpp>

void bts::delegate_plugin::delegate_plugin::configure(const bts::delegate_plugin::delegate_plugin::plugin_config& cfg)
{
   _config = cfg;

   if( !_config.delegate_keys.empty() )
      _block_production_task = fc::async([this]{block_production_loop();}, "Delegate Block Production");
}

void bts::delegate_plugin::delegate_plugin::block_production_loop()
{
   const chain::database& db = database();
   std::set<chain::delegate_id_type> delegates;

}
