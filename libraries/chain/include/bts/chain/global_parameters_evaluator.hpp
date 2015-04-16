#pragma once

#include <bts/chain/evaluator.hpp>

namespace bts { namespace chain {

class global_parameters_update_evaluator : public evaluator<global_parameters_update_evaluator>
{
   public:
      typedef global_parameters_update_operation operation_type;

      object_id_type do_evaluate( const global_parameters_update_operation& o );
      object_id_type do_apply( const global_parameters_update_operation& o );
};

} } // bts::chain
