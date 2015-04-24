#include <bts/account_history/account_history_plugin.hpp>

#include <bts/chain/time.hpp>
#include <bts/chain/operation_history_object.hpp>
#include <bts/chain/account_object.hpp>

#include <fc/thread/thread.hpp>

namespace bts { namespace account_history {

struct operation_get_impacted_accounts
{
   const operation_history_object& _op_history;
   const account_history_plugin&   _plugin;
   flat_set<account_id_type>&      _impacted;
   operation_get_impacted_accounts( const operation_history_object& oho, const account_history_plugin& ahp, flat_set<account_id_type>& impact )
      :_op_history(oho),_plugin(ahp),_impacted(impact)
   {}
   typedef void result_type;

   void add_authority( const authority& a )const
   {
      for( auto& item : a.auths )
      {
         if( item.first.type() == account_object_type )
            _impacted.insert( item.first );
      }
   }

   void operator()( const transfer_operation& o )const {
      _impacted.insert( o.to );
   }

   void operator()( const limit_order_create_operation& o )const { }
   void operator()( const short_order_create_operation& o )const { }
   void operator()( const limit_order_cancel_operation& o )const { }
   void operator()( const short_order_cancel_operation& o )const { }
   void operator()( const call_order_update_operation& o )const { }
   void operator()( const key_create_operation& o )const { }

   void operator()( const account_create_operation& o )const {
      _impacted.insert( _op_history.result.get<object_id_type>() );
   }

   void operator()( const account_update_operation& o )const {
      if( o.owner )
      {
         add_authority( *o.owner );
      }
      if( o.active )
      {
         add_authority( *o.active );
      }
   }
   void operator()( const account_transfer_operation& o )const
   {
      _impacted.insert( o.new_owner );
   }
   void operator()( const account_claim_cashback_operation& o )const{}

   void operator()( const account_whitelist_operation& o )const {
       _impacted.insert( o.account_to_list );
   }

   void operator()( const asset_create_operation& o )const {
   }

   void operator()( const asset_update_operation& o )const {
   }

   void operator()( const asset_issue_operation& o )const {
       _impacted.insert( o.issue_to_account );
   }
   void operator()( const asset_settle_operation& o )const {
   }

   void operator()( const asset_fund_fee_pool_operation& o )const { }
   void operator()( const delegate_publish_feeds_operation& o )const { }
   void operator()( const delegate_create_operation& o )const { }

   void operator()( const withdraw_permission_create_operation& o )const{
      _impacted.insert(o.authorized_account);
   }
   void operator()( const withdraw_permission_claim_operation& o )const{
      _impacted.insert( o.withdraw_from_account );
   }
   void operator()( const withdraw_permission_update_operation& o )const{
      _impacted.insert( o.authorized_account );
   }
   void operator()( const withdraw_permission_delete_operation& o )const{
      _impacted.insert( o.authorized_account );
   }

   void operator()( const witness_create_operation& o )const {
      _impacted.insert(o.witness_account);
   }

   void operator()( const witness_withdraw_pay_operation& o )const { }

   void operator()( const proposal_create_operation& o )const {
       for( auto op : o.proposed_ops )
          op.op.visit( operation_get_required_auths( _impacted, _impacted ) );
   }

   void operator()( const proposal_update_operation& o )const { }
   void operator()( const proposal_delete_operation& o )const { }

   void operator()( const fill_order_operation& o )const {
      _impacted.insert( o.account_id );
   }

   void operator()(const global_parameters_update_operation& )const {
      _impacted.insert( account_id_type() );
   }

   void operator()( const create_bond_offer_operation& o )const { }
};



void account_history_plugin::configure(const account_history_plugin::plugin_config& cfg)
{
   _config = cfg;
   database().applied_block.connect( [&]( const signed_block& b){ update_account_histories(b); } );
   database().add_index< primary_index< simple_index< operation_history_object > > >();
   database().add_index< primary_index< simple_index< account_transaction_history_object > > >();
}

void account_history_plugin::update_account_histories( const signed_block& b )
{
   chain::database& db = database();
   const vector<operation_history_object>& hist = db.get_applied_operations();
   for( auto op : hist )
   {
      // add to the operation history index
      const auto& oho = db.create<operation_history_object>( [&]( operation_history_object& h ){
                                h = op;
                        });

      // get the set of accounts this operation applies to
      flat_set<account_id_type> impacted;
      op.op.visit( operation_get_required_auths( impacted, impacted ) );
      op.op.visit( operation_get_impacted_accounts( oho, *this, impacted ) );

      // for each operation this account applies to that is in the config link it into the history
      if( _config.accounts.size() == 0 )
      {
         for( auto& account_id : impacted )
         {
            // add history
            const auto& stats_obj = account_id(db).statistics(db);
            const auto& ath = db.create<account_transaction_history_object>( [&]( account_transaction_history_object& obj ){
                obj.operation_id = oho.id;
                obj.next = stats_obj.most_recent_op;
            });
            db.modify( stats_obj, [&]( account_statistics_object& obj ){
                obj.most_recent_op = ath.id;
            });
         }
      }
      else
      {
         for( auto account_id : _config.accounts )
         {
            if( impacted.find( account_id ) != impacted.end() )
            {
               // add history
               const auto& stats_obj = account_id(db).statistics(db);
               const auto& ath = db.create<account_transaction_history_object>( [&]( account_transaction_history_object& obj ){
                   obj.operation_id = oho.id;
                   obj.next = stats_obj.most_recent_op;
               });
               db.modify( stats_obj, [&]( account_statistics_object& obj ){
                   obj.most_recent_op = ath.id;
               });
            }
         }
      }
   }
}
} }
