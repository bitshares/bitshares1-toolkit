#pragma once
#include <bts/chain/operations.hpp>

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
