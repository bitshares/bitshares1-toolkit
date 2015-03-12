#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

   class transfer_evaluator : public evaluator<transfer_evaluator>
   {
      public:
         typedef transfer_operation operation_type;

         object_id_type do_evaluate( const transfer_operation& o );
         object_id_type do_apply( const transfer_operation& o );
   };

} } // bts::chain
