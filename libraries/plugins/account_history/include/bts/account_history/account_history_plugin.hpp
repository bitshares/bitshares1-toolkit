#pragma once

#include <bts/app/plugin.hpp>
#include <bts/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace bts { namespace account_history {
using namespace chain;

class account_history_plugin : public bts::app::plugin<account_history_plugin> 
{
   public:
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

   private:
      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void update_account_histories( const signed_block& b );

      plugin_config _config;
};

} } //bts::account_history

FC_REFLECT( bts::account_history::account_history_plugin::plugin_config,
            (accounts)
           )
