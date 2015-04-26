#pragma once

#include <bts/app/plugin.hpp>
#include <bts/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace bts { namespace account_history {
using namespace chain;

namespace detail
{
    class account_history_plugin_impl;
}

class account_history_plugin : public bts::app::plugin<account_history_plugin> 
{
   public:
      account_history_plugin();
      virtual ~account_history_plugin();

      const std::string& plugin_name()const override {
         static std::string name = "account_history";
         return name;
      }

      struct plugin_config {
         /**
          *  If this is empty then all accounts will be tracked.
          */
         flat_set<account_id_type>  accounts;
      };

      void configure( const plugin_config& cfg );

      plugin_config _config;
      std::unique_ptr<detail::account_history_plugin_impl> _my;
};

} } //bts::account_history

FC_REFLECT( bts::account_history::account_history_plugin::plugin_config,
            (accounts)
           )
