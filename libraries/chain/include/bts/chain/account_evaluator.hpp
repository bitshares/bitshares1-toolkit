#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

   class account_create_evaluator : public evaluator<account_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;
   };

   class account_update_evaluator : public evaluator<account_update_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;


         const account_object*    acnt;
         vector<delegate_id_type> remove_votes;
         vector<delegate_id_type> add_votes;
   };

} } // bts::chain
