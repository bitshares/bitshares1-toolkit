#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

   class asset_create_evaluator : public evaluator<asset_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;

         share_type bts_fee_required;
   };

} } // bts::chain
