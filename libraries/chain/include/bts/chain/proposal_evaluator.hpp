#pragma once

#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>

namespace bts { namespace chain {

   class proposal_create_evaluator : public evaluator<proposal_create_evaluator>
   {
      public:
         typedef proposal_create_operation operation_type;

         object_id_type do_evaluate( const proposal_create_operation& o );
         object_id_type do_apply( const proposal_create_operation& o );

         transaction _proposed_trx;
   };

   class proposal_update_evaluator : public evaluator<proposal_update_evaluator>
   {
      public:
         typedef proposal_update_operation operation_type;

         object_id_type do_evaluate( const proposal_update_operation& o );
         object_id_type do_apply( const proposal_update_operation& o );

         const proposal_object* _proposal = nullptr;
         processed_transaction _processed_transaction;
         bool _executed_proposal = false;
         bool _proposal_failed = false;
   };

   class proposal_delete_evaluator : public evaluator<proposal_delete_evaluator>
   {
      public:
         typedef proposal_delete_operation operation_type;

         object_id_type do_evaluate( const proposal_delete_operation& o );
         object_id_type do_apply(const proposal_delete_operation&);

         const proposal_object* _proposal = nullptr;
   };

} } // bts::chain
