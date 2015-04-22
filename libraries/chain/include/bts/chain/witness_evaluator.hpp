#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/witness_object.hpp>

namespace bts { namespace chain {

   class witness_create_evaluator : public evaluator<witness_create_evaluator>
   {
      public:
         typedef witness_create_operation operation_type;

         object_id_type do_evaluate( const witness_create_operation& o );
         object_id_type do_apply( const witness_create_operation& o );
   };

   class witness_withdraw_pay_evaluator : public evaluator<witness_withdraw_pay_evaluator>
   {
      public:
         typedef witness_withdraw_pay_operation operation_type;

         object_id_type do_evaluate( const operation_type& o );
         object_id_type do_apply( const operation_type& o );

         const witness_object* witness;
         const account_object* to_account;
   };
} } // bts::chain
