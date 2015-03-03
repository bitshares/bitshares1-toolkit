
#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/delegate_object.hpp>

namespace bts { namespace chain {

   class delegate_create_evaluator : public evaluator<delegate_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;
   };
   class delegate_update_evaluator : public evaluator<delegate_create_operation>
   {
      public:
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;
   };

} } // bts::chain
