#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

   class asset_create_evaluator : public evaluator<asset_create_evaluator>
   {
      public:
         typedef asset_create_operation operation_type;

         object_id_type do_evaluate( const asset_create_operation& o );
         object_id_type do_apply( const asset_create_operation& o );

         share_type bts_fee_required;
   };

   class asset_whitelist_evaluator : public evaluator<asset_whitelist_evaluator>
   {
      public:
         typedef asset_whitelist_operation operation_type;

         object_id_type do_evaluate( const asset_whitelist_operation& o );
         object_id_type do_apply( const asset_whitelist_operation& o );

         share_type bts_fee_required;
         const account_object* whitelist_account = nullptr;
         const asset_object* whitelist_asset = nullptr;
   };

   class asset_issue_evaluator : public evaluator<asset_issue_evaluator>
   {
      public:
         typedef asset_issue_operation operation_type;
         object_id_type do_evaluate( const asset_issue_operation& o );
         object_id_type do_apply( const asset_issue_operation& o );

         share_type bts_fee_required;
         const asset_dynamic_data_object* asset_dyn_data = nullptr;
         const account_object*            to_account = nullptr;
   };

   class asset_fund_fee_pool_evaluator : public evaluator<asset_fund_fee_pool_evaluator>
   {
      public:
         typedef asset_fund_fee_pool_operation operation_type;

         object_id_type do_evaluate(const asset_fund_fee_pool_operation& op);
         object_id_type do_apply(const asset_fund_fee_pool_operation& op);

         share_type bts_fee_required;
         const asset_dynamic_data_object* asset_dyn_data = nullptr;
   };

} } // bts::chain
