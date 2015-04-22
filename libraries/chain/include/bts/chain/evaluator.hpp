#pragma once
#include <bts/chain/operations.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain {

   class database;
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
         virtual operation_result start_evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply  );

         /** @note derived classes should ASSUME that the default validation that is
          * indepenent of chain state should be performed by op.validate() and should
          * not perform these extra checks.
          */
         virtual operation_result evaluate( const operation& op ) = 0;
         virtual operation_result apply( const operation& op ) = 0;

         database& db()const;

         void check_required_authorities(const operation& op);
   protected:
         /** market helpers */

         /**
          *  Matches the two orders,
          *
          *  @return a bit field indicating which orders were filled (and thus removed)
          *
          *  0 - no orders were matched
          *  1 - bid was filled
          *  2 - ask was filled
          *  3 - both were filled
          */
         ///@{
         template<typename OrderType>
         int match( const limit_order_object& bid, const OrderType& ask, const price& match_price );
         int match( const limit_order_object& bid, const limit_order_object& ask, const price& trade_price );
         int match( const limit_order_object& bid, const short_order_object& ask, const price& trade_price );
         int match( const call_order_object& ask, const limit_order_object& );
         int match( const call_order_object& ask, const short_order_object& );
         ///@}

         /**
          * @return true if the order was completely filled and thus freed.
          */
         bool fill_order( const limit_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const short_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const call_order_object& order, const asset& pays, const asset& receives );

         bool convert_fees( const asset_object& mia );
         bool check_call_orders( const asset_object& mia );


         // helpers to fill_order
         void pay_order( const account_object& receiver, const asset& receives, const asset& pays );
         asset pay_market_fees( const asset_object& recv_asset, const asset& receives );

         /**
          *  Pays the fee and returns the number of CORE asset that were provided,
          *  after it is don, the fee_paying_account property will be set.
          */
         share_type pay_fee( account_id_type account_id, asset fee, bool is_prime_upgrade = false );
         bool       verify_authority( const account_object*, authority::classification );
         bool       verify_signature( const key_object* );

         /**
          *  Gets the balance of the account after all modifications that have been applied
          *  while evaluating this operation.
          */
         asset      get_balance( const account_object* for_account, const asset_object* for_asset )const;
         void       adjust_balance( const account_object* for_account, const asset_object* for_asset, share_type delta );
         void       adjust_votes(const flat_set<vote_tally_id_type>& vote_tallies, share_type delta );

         asset      calculate_market_fee( const asset_object& aobj, const asset& trade_amount );

         void       apply_delta_balances();
         void       apply_delta_fee_pools();

         object_id_type get_relative_id( object_id_type rel_id )const;

         authority resolve_relative_ids( const authority& a )const;

         struct fee_stats
         {
            share_type to_issuer;
            share_type from_pool;
            share_type to_accumulated_fees;
            share_type burned;
         };
         struct cash_back_stats
         {
            share_type cash_back;
            share_type total_fees_paid;
            bool       is_prime_upgrade;
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
         flat_map<const account_object*, cash_back_stats>                             cash_back;
         transaction_evaluation_state*                                                trx_state;



   };

   class op_evaluator
   {
      public:
         virtual ~op_evaluator(){}
         virtual operation_result evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply ) = 0;

         vector< shared_ptr<post_evaluator> > post_evals;
   };

   template<typename T>
   class op_evaluator_impl : public op_evaluator
   {
      public:
         virtual operation_result evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply = true ) override
         {
             T eval;
             auto result = eval.start_evaluate( eval_state, op, apply );
             for( const auto& pe : post_evals ) pe->post_evaluate( &eval );
             return result;
         }
   };

   template<typename DerivedEvaluator>
   class evaluator : public generic_evaluator
   {
      public:
         virtual int get_type()const { return operation::tag<typename DerivedEvaluator::operation_type>::value; }

         virtual operation_result evaluate( const operation& o ) final override
         {
            return static_cast<DerivedEvaluator*>(this)->do_evaluate( o.get<typename DerivedEvaluator::operation_type>() );
         }
         virtual operation_result apply( const operation& o ) final override
         {
            return static_cast<DerivedEvaluator*>(this)->do_apply( o.get<typename DerivedEvaluator::operation_type>() );
         }
   };
} }
