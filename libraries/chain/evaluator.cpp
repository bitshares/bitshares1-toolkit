#include<bts/chain/evaluator.hpp>
#include<bts/chain/transaction_evaluation_state.hpp>


namespace bts { namespace chain {
   database& generic_evaluator::db()const { return *trx_state->db; }
   object_id_type generic_evaluator::start_evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply )
   {
      trx_state   = &eval_state;
      auto result = evaluate( op );
      if( apply ) result = apply( op );
      return result;
   }
} }
