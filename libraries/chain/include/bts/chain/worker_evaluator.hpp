#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/worker_object.hpp>

namespace bts { namespace chain {

   class worker_create_evaluator : public evaluator<worker_create_evaluator>
   {
      public:
         typedef worker_create_operation operation_type;

         object_id_type do_evaluate( const operation_type& o );
         object_id_type do_apply( const operation_type& o );
   };

} } // bts::chain
