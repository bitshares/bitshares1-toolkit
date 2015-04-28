
#include <bts/account_history/account_history_plugin.hpp>

#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/config.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/evaluator.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/operation_history_object.hpp>
#include <bts/chain/account_object.hpp>

#include <fc/thread/thread.hpp>

namespace bts { namespace account_history {

namespace detail
{

class account_update_observer : public bts::chain::evaluation_observer
{
   public:
      account_update_observer( account_history_plugin& plugin )
          : _plugin( plugin )
      {
         _pre_account_keys.reserve( BTS_DEFAULT_MAX_AUTHORITY_MEMBERSHIP * 2 + 2 );
      }
      virtual ~account_update_observer();

      virtual void pre_evaluate(
          const transaction_evaluation_state& eval_state,
          const operation& op,
          bool apply,
          generic_evaluator* ge ) override;

      virtual void post_evaluate(
          const transaction_evaluation_state& eval_state,
          const operation& op,
          bool apply,
          generic_evaluator* ge ) override;

      virtual void evaluation_failed(
          const transaction_evaluation_state& eval_state,
          const operation& op,
          bool apply,
          generic_evaluator* ge ) override;

      account_history_plugin& _plugin;
      flat_set< key_id_type > _pre_account_keys;
};

class account_history_plugin_impl
{
   public:
      account_history_plugin_impl(
         account_history_plugin& _plugin
         ) : _self( _plugin ),
             _update_observer( _plugin ),
             _chain_db( nullptr )
         { }
      virtual ~account_history_plugin_impl();

      flat_set<key_id_type> get_keys_for_account(
          const account_id_type& account_id );

      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void update_account_histories( const signed_block& b );
      void index_account_keys( const account_id_type& account_id );
      // protected in abstract_plugin interface,
      //  we publish it here...if you can see this impl class,
      //  you know what you doing for great justice
      bts::chain::database& database()
      {
         assert( _chain_db != nullptr );
         return *_chain_db;
      }

      account_history_plugin& _self;
      account_update_observer _update_observer;
      bts::chain::database* _chain_db;
};

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

account_update_observer::~account_update_observer()
{
    return;
}

void account_update_observer::pre_evaluate(
    const transaction_evaluation_state& eval_state,
    const operation& op,
    bool apply,
    generic_evaluator* ge )
{
   FC_ASSERT( op.which() == operation::tag<account_update_operation>::value );

   // if we only care about given accounts, then key -> account mapping
   //   is not maintained
   if( _plugin._config.accounts.size() > 0 )
       return;

   // is this a tx which affects a key?
   // switch( op.which() )
   // {
   // see note in configure() why account_create_operation handling is unnecessary
   //   case operation::tag<account_create_operation>::value:
   //      const account_create_operation& create_op = op.get< account_create_operation >();
   //      break;
   //   case operation::tag<account_update_operation>::value:
   //      const account_update_operation& update_op = op.get< account_update_operation >();
   //      _pre_account_keys.clear();
   //      get_updatable_account_keys( update_op, _pre_account_keys );
   //      break;
   //   default:
   //      FC_ASSERT( false, "account_update_observer got unexpected operation type" );
   //}

   const account_update_operation& update_op = op.get< account_update_operation >();
   _pre_account_keys = _plugin._my->get_keys_for_account( update_op.account );
   return;
}

void account_update_observer::post_evaluate(
    const transaction_evaluation_state& eval_state,
    const operation& op,
    bool apply,
    generic_evaluator* ge
    )
{
   FC_ASSERT( op.which() == operation::tag<account_update_operation>::value );

   bts::chain::database& db = _plugin._my->database();

   // if we only care about given accounts, then key -> account mapping
   //   is not maintained
   if( _plugin._config.accounts.size() > 0 )
       return;

   // wild back-of-envelope heuristic:  most account update ops
   //   don't add more than two keys
   flat_set<key_id_type> post_account_keys;
   post_account_keys.reserve( _pre_account_keys.size() + 2 );

   vector<key_id_type> removed_account_keys;
   removed_account_keys.reserve( _pre_account_keys.size() );

   const account_update_operation& update_op = op.get< account_update_operation >();
   post_account_keys = _plugin._my->get_keys_for_account( update_op.account );

   std::set_difference(
      _pre_account_keys.begin(), _pre_account_keys.end(),
      post_account_keys.begin(), post_account_keys.end(),
      removed_account_keys.begin()
      );

   //
   // If a key_id is in _pre_account_keys but not in post_account_keys,
   //    then it is placed in removed_account_keys by set_difference().
   //
   // Note, the *address* corresponding to this key may still exist
   //    in the account, because it may be aliased to multiple key_id's.
   //
   // We delete the key_account_object for all removed_account_keys.
   //    This correctly deletes addresses which were removed
   //    from the account by the update_op.
   //
   // Unfortunately, in the case of aliased keys, it deletes
   //    key_account_object if *any* of the aliases was removed from
   //    the account.  We want it to delete only if *all* of the aliases
   //    were removed from the account.
   //
   // However, we need to run index_account_keys() afterwards anyway.
   //    It will re-add to the index any addresses which had been
   //    deleted -- but only if they still exist in the account under
   //    at least one alias.
   //
   // This is precisely the correct behavior.
   //

   for( const key_id_type& key_id : removed_account_keys )
   {
      auto& index = db.get_index_type<key_account_index>().indices().get<by_key>();
      auto it = index.find( key_id(db).key_address() );
      assert( it != index.end() );

      db.modify<key_account_object>( *it, [&]( key_account_object& ka )
      {
         ka.account_ids.erase( update_op.account );
      });
   }
   return;
}

void account_update_observer::evaluation_failed(
    const transaction_evaluation_state& eval_state,
    const operation& op,
    bool apply,
    generic_evaluator* ge
    )
{
   // if we only care about given accounts, then key -> account mapping
   //   is not maintained
   if( _plugin._config.accounts.size() > 0 )
       return;

   // cleaning up here is not strictly necessary, but good "hygiene"
   _pre_account_keys.clear();
   return;
}

account_history_plugin_impl::~account_history_plugin_impl()
{
   return;
}

flat_set<key_id_type> account_history_plugin_impl::get_keys_for_account( const account_id_type& account_id )
{
   const bts::chain::database& db = database();
   const account_object& acct = account_id(db);

   const flat_map<object_id_type, weight_type>& owner_auths =
       acct.owner.auths;
   const flat_map<object_id_type, weight_type>& active_auths =
       acct.active.auths;

   flat_set<key_id_type> key_id_set;
   key_id_set.reserve(owner_auths.size() + active_auths.size() + 2);

   key_id_set.insert(acct.memo_key);

   // we don't use get_keys() here to avoid an intermediate copy operation
   for( const pair<object_id_type, weight_type>& item : active_auths )
   {
      if( item.first.type() == key_object_type )
         key_id_set.insert( item.first );
   }

   for( const pair<object_id_type, weight_type>& item : owner_auths )
   {
      if( item.first.type() == key_object_type )
         key_id_set.insert( item.first );
   }

   return key_id_set;
}

void account_history_plugin_impl::index_account_keys( const account_id_type& account_id )
{
   // for each key in account authority... get address, modify(..)
   bts::chain::database& db = database();

   flat_set<key_id_type> key_id_set = get_keys_for_account( account_id );

   flat_set<address> address_set;

   //
   // we pass the addresses through another flat_set because the
   //    blockchain doesn't force de-duplication of addresses
   //    (multiple key_id's might refer to the same address)
   //
   address_set.reserve( key_id_set.size() );

   for( const key_id_type& key_id : key_id_set )
      address_set.insert( key_id(db).key_address() );

   // add mappings for the given account
   for( const address& addr : address_set )
   {
       auto& idx = db.get_index_type<key_account_index>().indices().get<by_key>();
       auto it = idx.find( addr );
       if( it == idx.end() )
       {
          // if unknown, we need to create a new object
          db.create<key_account_object>( [&]( key_account_object& ka )
          {
             ka.key = addr;
             ka.account_ids.insert( account_id );
          });
       }
       else
       {
          // if known, we need to add to existing object
          db.modify<key_account_object>( *it,
             [&]( key_account_object& ka )
             {
                ka.account_ids.insert( account_id );
             });
       }
   }

   return;
}

void account_history_plugin_impl::update_account_histories( const signed_block& b )
{
   bts::chain::database& db = database();
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
      op.op.visit( operation_get_impacted_accounts( oho, _self, impacted ) );

      // for each operation this account applies to that is in the config link it into the history
      if( _self._config.accounts.size() == 0 )
      {
         for( auto& account_id : impacted )
         {
            index_account_keys( account_id );

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
         for( auto account_id : _self._config.accounts )
         {
            if( impacted.find( account_id ) != impacted.end() )
            {
               index_account_keys( account_id );

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
} // end namespace detail

account_history_plugin::account_history_plugin() :
   _my( new detail::account_history_plugin_impl(*this) )
{
   return;
}

account_history_plugin::~account_history_plugin()
{
   return;
}

void account_history_plugin::configure(const account_history_plugin::plugin_config& cfg)
{
   _config = cfg;
   database().applied_block.connect( [&]( const signed_block& b){ _my->update_account_histories(b); } );
   database().add_index< primary_index< simple_index< operation_history_object > > >();
   database().add_index< primary_index< simple_index< account_transaction_history_object > > >();
   database().add_index< primary_index< key_account_index >>();
   // account_create_operation is actually unnecessary
   //    because all the necessary keys will be touched when
   //    update_account_histories() is called, unless they are removed
   //
   // and the only way they can be removed is by account_update_operation
   //    which will mark the removed keys for updating
   //
   // database().register_evaluation_observer<account_create_evaluator>( _my->_observer );
   database().register_evaluation_observer< bts::chain::account_update_evaluator >( _my->_update_observer );
}

void account_history_plugin::init()
{
   _my->_chain_db = &database();
   return;
}


} }
