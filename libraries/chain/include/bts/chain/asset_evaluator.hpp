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

   class asset_issue_evaluator : public evaluator<asset_issue_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;

         share_type bts_fee_required;
         const asset_dynamic_data_object* asset_dyn_data = nullptr;
         const account_object*            to_account = nullptr;
   };

   class asset_fund_fee_pool_evaluator : public evaluator<asset_fund_fee_pool_operation>
   {
      public:
         virtual object_id_type evaluate(const operation& op) override;
         virtual object_id_type apply(const operation& op) override;

         share_type bts_fee_required;
         const asset_dynamic_data_object* asset_dyn_data = nullptr;
   };

} } // bts::chain
