#pragma once
#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

   class key_create_evaluator : public evaluator<key_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override
         {
            const auto& op = o.get<key_create_operation>();
            auto bts_fee_paid = pay_fee( op.fee_paying_account, op.fee );
            auto bts_fee_required = db().get_global_properties()->current_fees.at( key_creation_fee_type );
            FC_ASSERT( bts_fee_paid >= bts_fee_required );

            return object_id_type();
         }

         virtual object_id_type apply( const operation& o ) override
         {
            apply_delta_balances();
            apply_delta_fee_pools();

            const auto& op = o.get<key_create_operation>();
            new_key_object = db().create<key_object>( [&]( key_object* obj ){
                obj->key_data = op.key_data;
            });
            FC_ASSERT( new_key_object );
            return new_key_object->id;
         }

         const key_object*                new_key_object = nullptr;
   };

} } // bts::chain
