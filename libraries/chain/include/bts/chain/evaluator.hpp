#pragma once
#include <bts/chain/operations.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain {

   class generic_evaluator;
   class transaction_evaluation_state;

   class post_evaluator
   {
      public:
         virtual ~post_evaluator(){}
         virtual void post_evaluate( generic_evaluator* ge )const = 0;
   };

   class generic_evaluator
   {
      public:
         virtual ~generic_evaluator(){}

         virtual int get_type()const = 0;
         virtual object_id_type start_evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply  );

         virtual object_id_type evaluate( const operation& op ) = 0;
         virtual object_id_type apply( const operation& op ) = 0;

         database& db()const;

      protected:
         /**
          *  Pays the fee and returns the number of CORE asset that were provided,
          *  after it is don, the fee_paying_account property will be set.
          */
         share_type pay_fee( account_id_type account_id, asset fee );
         bool       verify_authority( const account_object*, authority::classification );

         /**
          *  Gets the balance of the account after all modifications that have been applied 
          *  while evaluating this operation.
          */
         asset      get_balance( const account_object* for_account, const asset_object* for_asset )const;
         void       adjust_balance( const account_object* for_account, const asset_object* for_asset, share_type delta );

         void       apply_delta_balances();
         void       apply_delta_fee_pools();

         struct fee_stats 
         { 
            share_type to_issuer; 
            share_type from_pool;
         };

         const account_object*            fee_paying_account = nullptr;
         const account_balance_object*    fee_paying_account_balances = nullptr;
         const asset_object*              fee_asset          = nullptr;
         const asset_dynamic_data_object* fee_asset_dyn_data = nullptr;

         /** Tracks the total fees paid in each asset type and the
          * total amount taken from the fee pool of that asset.
          */
         flat_map<const asset_object*, fee_stats>                                     fees_paid;
         flat_map< const account_object*, flat_map<const asset_object*, share_type> > delta_balance;
         transaction_evaluation_state*                                                trx_state;
   };

   class op_evaluator
   {
      public:
         virtual ~op_evaluator(){}
         virtual object_id_type evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply ) = 0;
         vector< shared_ptr<post_evaluator> > post_evals;
   };

   template<typename T>
   class op_evaluator_impl : public op_evaluator
   {
      public:
         virtual object_id_type evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply = true ) override
         {
             T eval;
             auto result = eval.start_evaluate( eval_state, op, apply );
             for( const auto& pe : post_evals ) pe->post_evaluate( &eval );
             return result;
         }
   };

   template<typename OperationClass>
   class evaluator : public generic_evaluator
   {
      public:
         typedef OperationClass  operation_class_type;
         virtual int get_type()const { return operation::tag<operation_class_type>::value; }
   };
} }
