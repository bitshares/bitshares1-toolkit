#include <bts/chain/transfer_evaluator.hpp>

namespace bts { namespace chain {
object_id_type transfer_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<transfer_operation>();
   auto bts_fee_paid = pay_fee( op.from, op.fee );
   auto bts_fee_required = db().current_fee( transfer_fee_type );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   return object_id_type();
}

object_id_type transfer_evaluator::apply( const operation& o ) 
{
   apply_delta_balances();
   apply_delta_fee_pools();

   return object_id_type();
}
} } // bts::chain 
