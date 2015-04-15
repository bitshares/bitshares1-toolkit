#include <bts/account_history/account_history_plugin.hpp>

#include <bts/chain/time.hpp>
#include <bts/chain/operation_history_object.hpp>

#include <fc/thread/thread.hpp>

namespace bts { namespace account_history {

void account_history_plugin::configure(const account_history_plugin::plugin_config& cfg)
{
   _config = cfg;
   database().applied_block.connect( [&]( const signed_block& b){ update_account_histories(b); } );
}

void account_history_plugin::update_account_histories( const signed_block& b )
{
   chain::database& db = database();
   const vector<operation_history_object>& hist = db.get_applied_operations();
   for( auto op : hist )
   {
      // add to the operation history index

      // get the set of accounts this operation applies to

      // for each operation this account applies to that is in the config link it into the history
   }
}
} }
