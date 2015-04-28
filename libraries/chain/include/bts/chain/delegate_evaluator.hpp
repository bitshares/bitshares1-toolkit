#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/delegate_object.hpp>

namespace bts { namespace chain {

   class delegate_create_evaluator : public evaluator<delegate_create_evaluator>
   {
      public:
         typedef delegate_create_operation operation_type;

         object_id_type do_evaluate( const delegate_create_operation& o );
         object_id_type do_apply( const delegate_create_operation& o );
   };

} } // bts::chain
