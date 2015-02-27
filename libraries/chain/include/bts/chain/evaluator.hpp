#pragma once

namespace bts { namespace chain {

   class generic_evaluator;
   class post_evaluator
   {
      public:
         virtual ~post_evaluator(){};
         virtual void post_evaluate( generic_evaluator* ge )const = 0;
   };

   class generic_evaluator
   {
      public:
         virtual ~generic_evaluator();

         virtual operation_type get_type()const = 0;
         virtual object_id_type evaluate( transaction_evaluation_state& eval_state, const operation& op )
         {
            trx_state   = &eval_state;
            current_op  = &op;
            auto result = evaluate( op );
            return result;
         }
         virtual object_id_type evaluate( const operation& op ) = 0;
         virtual object_id_type evaluate() = 0;
         virtual object_id_type apply() = 0;

      protected:
         const operation*                     current_op;
         transaction_evaluation_state*        trx_state;
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
         virtual object_id_type evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply = true )
         {
             T eval;
             auto result = eval.evaluate( eval_state, op );
             if( apply )
                result = eval.apply();
             for( const auto& pe : post_evals )
                pe->post_evaluate( &eval );
             return result;
         }
   };


   template<typename OperationClass>
   class evaluator : public generic_evaluator
   {
      public:
         typedef OperationClass  operation_class_type;

         virtual operation_type get_type()const { return operation_class_type::type; }

         virtual object_id_type evaluate( const operation& gop ) override
         {
            current_op = &gop;
            _op = gop.as<operation_class_type>();
            return evaluate();
         }
         const OperationClass& op()const { return _op; }

      private:
         operation_class_type  _op;
   };

   /*
   class create_account_evaluator : public evaluator<create_account_operation>
   {
      public:
         typedef create_account_operation operation_class_type;

         virtual object_id_type evaluate()
         {
            op().field... 
         }

         virtual object_id_type apply()
         {
         }
         all of my state variables here... that normally exist on the stack.
   };
   class advanced_create_account_eval : public create_account_evaluator
   {
         virtual object_id_type evaluate()
         {
            base::evaluate();
            op().field... 
         }

         virtual object_id_type apply()
         {
           base::apply();
           ... 
         }
   };
   */

} }
