#pragma once

#include <bts/app/plugin.hpp>
#include <bts/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace bts { namespace delegate_plugin {

class delegate_plugin : public bts::app::plugin<delegate_plugin> {
public:
   ~delegate_plugin() {
      if( _block_production_task.valid() )
         _block_production_task.cancel_and_wait(__FUNCTION__);
   }

   const std::string& plugin_name()const override {
      static std::string name = "delegate";
      return name;
   }

   struct plugin_config {
      std::map<bts::chain::delegate_id_type, fc::ecc::private_key> delegate_keys;
   };

   void configure( const plugin_config& cfg );

private:
   void block_production_loop();

   plugin_config _config;
   fc::future<void> _block_production_task;
};

} } //bts::delegate

FC_REFLECT( bts::delegate_plugin::delegate_plugin::plugin_config, (delegate_keys) )
