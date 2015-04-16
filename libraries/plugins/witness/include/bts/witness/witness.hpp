#pragma once

#include <bts/app/plugin.hpp>
#include <bts/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace bts { namespace witness_plugin {

class witness_plugin : public bts::app::plugin<witness_plugin> {
public:
   ~witness_plugin() {
      try {
         if( _block_production_task.valid() )
            _block_production_task.cancel_and_wait(__FUNCTION__);
      } catch(fc::canceled_exception&) {
         //Expected exception. Move along.
      } catch(fc::exception& e) {
         edump((e.to_detail_string()));
      }
   }

   const std::string& plugin_name()const override {
      static std::string name = "delegate";
      return name;
   }

   void set_block_production(bool allow) { _production_enabled = allow; }

   struct plugin_config {
      std::map<bts::chain::witness_id_type, fc::ecc::private_key> witness_keys;
      /// Only set to true when starting a new network, or all delegates are offline.
      bool allow_production_on_stale_chain = false;
   };

   void configure( const plugin_config& cfg );

private:
   void block_production_loop();

   /// This will be set to false until we see a head block at time now (give or take an interval)
   /// Suppress this behavior by setting allow_production_on_stale_chain to true in the config file.
   bool _production_enabled = false;
   plugin_config _config;
   fc::future<void> _block_production_task;
};

} } //bts::delegate

FC_REFLECT( bts::witness_plugin::witness_plugin::plugin_config,
            (witness_keys)(allow_production_on_stale_chain)
           )
