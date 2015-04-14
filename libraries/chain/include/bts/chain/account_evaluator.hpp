#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

   class account_create_evaluator : public evaluator<account_create_evaluator>
   {
      public:
         typedef account_create_operation operation_type;

         object_id_type do_evaluate( const account_create_operation& o );
         object_id_type do_apply( const account_create_operation& o ) ;
   };

   class account_update_evaluator : public evaluator<account_update_evaluator>
   {
      public:
         typedef account_update_operation operation_type;

         object_id_type do_evaluate( const account_update_operation& o );
         object_id_type do_apply( const account_update_operation& o );

         const account_object*    acnt;
         vector<delegate_id_type> remove_votes;
         vector<delegate_id_type> add_votes;
   };

   class account_whitelist_evaluator : public evaluator<account_whitelist_evaluator>
   {
      public:
         typedef account_whitelist_operation operation_type;

         object_id_type do_evaluate( const account_whitelist_operation& o);
         object_id_type do_apply( const account_whitelist_operation& o);

         const account_object* listed_account;
   };

} } // bts::chain
