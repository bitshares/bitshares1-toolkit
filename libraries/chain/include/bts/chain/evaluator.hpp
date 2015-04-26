#pragma once
#include <bts/chain/operations.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain {

   class database;
   class generic_evaluator;
   class transaction_evaluation_state;

   /**
    * Observes evaluation events, providing
    * pre- and post-evaluation hooks.
    *
    * Every call to pre_evaluate() is followed by
    * a call to either post_evaluate() or evaluation_failed().
    *
    * A subclass which needs to do a "diff" can gather some
    * "before" state into its members in pre_evaluate(),
    * then post_evaluate() will have both "before"
    * and "after" state, and will be able to do the diff.
    *
    * evaluation_failed() is a cleanup method which notifies
    * the subclass to "throw away" the diff.
    */
   class evaluation_observer
   {
      public:
         virtual ~evaluation_observer(){}

         virtual void pre_evaluate(
             const transaction_evaluation_state& eval_state,
             const operation& op,
             bool apply,
             generic_evaluator* ge ) {}

         virtual void post_evaluate(
             const transaction_evaluation_state& eval_state,
             const operation& op,
             bool apply,
             generic_evaluator* ge ) {}

         virtual void evaluation_failed(
             const transaction_evaluation_state& eval_state,
             const operation& op,
             bool apply,
             generic_evaluator* ge ) {}
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
         void settle_black_swan( const asset_object& bitasset, const price& settle_price );
         void cancel_order( const limit_order_object& order, bool create_virtual_op = true );

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
         int match( const call_order_object& call, const force_settlement_object& settle , const price& match_price );
         int match( const call_order_object& ask, const short_order_object& );
         ///@}

         /**
          * @return true if the order was completely filled and thus freed.
          */
         bool fill_order( const limit_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const short_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const call_order_object& order, const asset& pays, const asset& receives );
         bool fill_order( const force_settlement_object& settle, const asset& pays, const asset& receives );

         bool convert_fees( const asset_object& mia );
         bool check_call_orders( const asset_object& mia );

         // helpers to fill_order
         void pay_order( const account_object& receiver, const asset& receives, const asset& pays );
         asset pay_market_fees( const asset_object& recv_asset, const asset& receives );

         asset get_balance(const account_object& account_obj, const asset_object& asset_obj);
         void  adjust_balance(account_id_type account, asset delta);

         /**
          * @brief Fetch objects relevant to fee payer and set pointer members
          * @param account_id Account which is paying the fee
          * @param fee The fee being paid. May be in assets other than core.
          *
          * This method verifies that the fee is valid and sets the object pointer members and the fee fields. It should
          * be called during do_evaluate.
          */
         void prepare_fee(account_id_type account_id, asset fee);
         /// Pays the fee and returns the number of CORE asset that were paid.
         void pay_fee();

         bool       verify_authority( const account_object&, authority::classification );
         bool       verify_signature( const key_object& );

         asset      calculate_market_fee( const asset_object& aobj, const asset& trade_amount );

         object_id_type get_relative_id( object_id_type rel_id )const;

         authority resolve_relative_ids( const authority& a )const;

         asset                            fee_from_account;
         share_type                       core_fee_paid;
         const account_object*            fee_paying_account = nullptr;
         const account_statistics_object* fee_paying_account_statistics = nullptr;
         const asset_object*              fee_asset          = nullptr;
         const asset_dynamic_data_object* fee_asset_dyn_data = nullptr;
         transaction_evaluation_state*    trx_state;
   };

   class op_evaluator
   {
      public:
         virtual ~op_evaluator(){}
         virtual operation_result evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply ) = 0;

         vector< evaluation_observer* > eval_observers;
   };

   template<typename T>
   class op_evaluator_impl : public op_evaluator
   {
      public:
         virtual operation_result evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply = true ) override
         {
             // fc::exception from observers are suppressed.
             // fc::exception from evaluation is deferred (re-thrown
             // after all observers receive evaluation_failed)

             T eval;
             optional< fc::exception > evaluation_exception;
             size_t observer_count = 0;
             decltype( eval.start_evaluate( eval_state, op, apply ) ) result;

             for( const auto& obs : eval_observers )
             {
                try
                {
                   obs->pre_evaluate( eval_state, op, apply, &eval );
                }
                catch( const fc::exception& e )
                {
                   elog( "suppressed exception in observer pre method:\n${e}", ( "e", e.to_detail_string() ) );
                }
                observer_count++;
             }

             try
             {
                result = eval.start_evaluate( eval_state, op, apply );
             }
             catch( const fc::exception& e )
             {
                evaluation_exception = e;
                elog( "deferred evaluation_exception:\n${e}", ( "e", e.to_detail_string() ) );
             }

             while( observer_count > 0 )
             {
                --observer_count;
                const auto& obs = eval_observers[ observer_count ];
                try
                {
                   if( !evaluation_exception.valid() )
                      obs->post_evaluate( eval_state, op, apply, &eval );
                   else
                      obs->evaluation_failed( eval_state, op, apply, &eval );
                }
                catch( const fc::exception& e )
                {
                   elog( "suppressed exception in observer post method:\n${e}", ( "e", e.to_detail_string() ) );
                }
             }

             if( evaluation_exception.valid() )
                throw *evaluation_exception;
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
            auto* eval = static_cast<DerivedEvaluator*>(this);
            const auto& op = o.get<typename DerivedEvaluator::operation_type>();

            prepare_fee(op.fee_payer(), op.fee);
            FC_ASSERT( core_fee_paid >= op.calculate_fee(db().current_fee_schedule()) );

            return eval->do_evaluate( op );
         }
         virtual operation_result apply( const operation& o ) final override
         {
            auto* eval = static_cast<DerivedEvaluator*>(this);
            const auto& op = o.get<typename DerivedEvaluator::operation_type>();

            pay_fee();

            auto result = eval->do_apply( op );

            adjust_balance(op.fee_payer(), -fee_from_account);

            return result;
         }
   };
} }
