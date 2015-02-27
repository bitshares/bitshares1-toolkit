#pragma once
#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

   class key_create_evaluator : public evaluator<key_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override
         {
            const auto& op = o.get<key_create_operation>();
            fee_paying_account = op.fee_paying_account(db());
            FC_ASSERT( fee_paying_account != nullptr );
            fee_paying_account_balance = fee_paying_account->balances(db());
            FC_ASSERT( fee_paying_account_balance );
            FC_ASSERT( fee_paying_account_balance->get_balance( op.fee.asset_id ) >= op.fee );
            fee_asset = op.fee.asset_id(db());
            FC_ASSERT( fee_asset );
            fee_asset_dyn_data = fee_asset->dynamic_asset_data_id(db());
            FC_ASSERT( fee_asset_dyn_data );
            return object_id_type();
         }

         virtual object_id_type apply( const operation& o ) override
         {
            const auto& op = o.get<key_create_operation>();
            new_key_object = db().create<key_object>( [&]( key_object* obj ){
                obj->key_data = op.key_data;
            });
            FC_ASSERT( new_key_object );
            return new_key_object->id;
         }

         const account_object*            fee_paying_account = nullptr;
         const account_balance_object*    fee_paying_account_balance = nullptr;
         const asset_object*              fee_asset = nullptr;
         const asset_dynamic_data_object* fee_asset_dyn_data = nullptr;
         const key_object*                new_key_object = nullptr;
   };

} } // bts::chain
