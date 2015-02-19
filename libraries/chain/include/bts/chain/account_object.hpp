#pragma once
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>

namespace bts { namespace chain {
   class account_balance_object : public object
   {
      public:
         static const object_type type = account_balance_object_type;
         account_balance_object():object( account_balance_object_type ){};

         void                  add_balance( const asset& a );
         void                  sub_balance( const asset& a );
         asset                 get_balance( asset_id_type asset_id )const;
         /**
          * Keep balances sorted for best performance of lookups in log(n) time,
          * balances need to be moved to their own OBJECT ID because they
          * will change all the time and are much smaller than an account.
          *
          * At 1000 TPS each referencing 2 accounts and each account representing
          * 1 MB in memory that represents 2 MB / second in serialization overhead
          * for the undo buffer.  
          */
         vector<pair<asset_id_type,share_type> > balances;
   };

   class account_object : public object
   {
      public:
         static const object_type type = account_object_type;

         account_object():object( account_object_type ){};

         bool                  is_for_sale()const { return for_sale.first != 0; }

         void                  authorize_asset( asset_id_type asset_id, bool state );
         bool                  is_authorized_asset( asset_id_type )const;

         string                name;
         authority             owner;
         authority             active;
         authority             voting;

         vector<delegate_id_type>      delegate_votes;

         /**
          *  If the account is for sale, list the price and account that
          *  should be paid.  If the account that should be paid is 0 then
          *  this account is not for sale.
          */
         pair<account_id_type, asset>  for_sale;

         object_id_type                          balances;
         vector<asset_id_type>                   authorized_assets;
         delegate_id_type                        delegate_id; // optional
   };

}} 
FC_REFLECT_DERIVED( bts::chain::account_object, 
                    (bts::chain::object), 
                    (name)(owner)(active)(voting)(delegate_votes)(for_sale)(balances)(authorized_assets)(delegate_id) )

FC_REFLECT_DERIVED( bts::chain::account_balance_object, (bts::chain::object), (balances) )
