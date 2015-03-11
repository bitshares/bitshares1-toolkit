#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

   class limit_order_evaluator : public evaluator<limit_order_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;

         asset calculate_market_fee( const asset_object* aobj, const asset& trade_amount );

         const limit_order_create_operation* _op            = nullptr;
         const account_object*               _seller        = nullptr;
         const asset_object*                 _sell_asset    = nullptr;
         const asset_object*                 _receive_asset = nullptr;
   };

} } // bts::chain
