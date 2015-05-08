#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

   class custom_evaluator : public evaluator<custom_evaluator>
   {
      public:
         typedef custom_operation operation_type;

         object_id_type do_evaluate( const custom_operation& o ){ return object_id_type(); }
         object_id_type do_apply( const custom_operation& o ){ return object_id_type(); }
   };
} }
