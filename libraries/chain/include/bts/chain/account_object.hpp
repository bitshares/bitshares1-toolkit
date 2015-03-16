#pragma once
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/generic_index.hpp>

namespace bts { namespace chain {
   /**
    * @class account_feeds_object
    * @brief tracks price feeds published by a particular account
    *
    */
   class account_feeds_object : public abstract_object<account_feeds_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_feeds_object_type;

         optional<price_feed> get_feed( asset_id_type quote, asset_id_type base )const;
         void                 set_feed( const price_feed& p );

         /**
          * Maps feeds to the last time they were updated.
          */
         flat_map<price_feed,time_point_sec> feeds;
   };

   /**
    *  @class account_balance_object
    *  @ingroup implementation
    *
    *  This object is provided for the purpose of separating the account data that
    *  changes frequently from the account data that is mostly static.  This will
    *  minimize the amount of data that must be backed up as part of the undo
    *  history everytime a transfer is made.  
    *
    *  Note: a single account with 1000 different asset types will require
    *  16KB in the undo buffer... this could significantly degrade performance
    *  at a large scale.  A future optimization would be to have a balance
    *  object for each asset type or at the very least group assets into
    *  smaller numbers.  
    */
   class account_balance_object : public abstract_object<account_balance_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_balance_object_type;

         void                  add_balance( const asset& a );
         void                  sub_balance( const asset& a );
         asset                 get_balance( asset_id_type asset_id )const;

         /**
          *  When calculating votes it is necessary to know how much is
          *  stored in orders (and thus unavailable for transfers).  Rather
          *  than maintaining an index of  [asset,owner,order_id] we will
          *  simply maintain the running total here and update it every
          *  time an order is created or modified.
          */
         share_type            total_core_in_orders;

         /**
          * Keep balances sorted for best performance of lookups in log(n) time,
          * balances need to be moved to their own OBJECT ID because they
          * will change all the time and are much smaller than an account.
          */
         vector<pair<asset_id_type,share_type> > balances;
   };

   class account_debt_object : public abstract_object<account_debt_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_debt_object_type;

         optional<call_order_id_type> get_call_order(asset_id_type aid )const
         {
            auto itr = call_orders.find(aid);
            if( itr != call_orders.end() ) return itr->second;
            return optional<call_order_id_type>();
         }

         flat_map<asset_id_type, call_order_id_type> call_orders;
   };

   class account_object : public annotated_object<account_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = account_object_type;

         const string& get_name()const { return name; }

         void                  authorize_asset( asset_id_type asset_id, bool state );
         bool                  is_authorized_asset( asset_id_type )const;

         string                name;
         authority             owner;
         authority             active;
         key_id_type           memo_key;
         key_id_type           voting_key;

         vector<delegate_id_type> delegate_votes;

         optional<account_feeds_id_type> feeds;
         account_balance_id_type         balances;
         account_debt_id_type            debts;
         flat_set<asset_id_type>         authorized_assets;
   };

   /**
    *  This object is attacked as the meta annotation on the account object, this
    *  information is not relevant to validation.
    */
   class meta_account_object : public abstract_object<meta_account_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = meta_account_object_type;

         key_id_type         memo_key;
         delegate_id_type    delegate_id; // optional
   };

   struct by_name{};
   typedef multi_index_container<
      account_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_non_unique< tag<by_name>, member<account_object, string, &account_object::name> >
      >
   > account_object_multi_index_type;

   typedef generic_index<account_object, account_object_multi_index_type> account_index;

}} 
FC_REFLECT_DERIVED( bts::chain::account_object, 
                    (bts::chain::annotated_object<bts::chain::account_object>), 
                    (name)(owner)(active)(memo_key)(voting_key)(delegate_votes)(feeds)(balances)(debts)(authorized_assets) )

FC_REFLECT_DERIVED( bts::chain::meta_account_object, 
                    (bts::chain::object), 
                    (memo_key)(delegate_id) )

FC_REFLECT_DERIVED( bts::chain::account_balance_object, (bts::chain::object), (total_core_in_orders)(balances) )
FC_REFLECT_DERIVED( bts::chain::account_debt_object, (bts::chain::object), (call_orders) );
