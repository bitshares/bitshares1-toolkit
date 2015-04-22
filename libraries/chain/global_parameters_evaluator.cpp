#include <bts/chain/global_parameters_evaluator.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

object_id_type global_parameters_update_evaluator::do_evaluate(const global_parameters_update_operation& o)
{
   FC_ASSERT(trx_state->_is_proposed_trx);

   auto fee_paid = pay_fee(account_id_type(), o.fee);
   FC_ASSERT(fee_paid >= o.calculate_fee(db().current_fee_schedule()));

   return object_id_type();
}

object_id_type global_parameters_update_evaluator::do_apply(const global_parameters_update_operation& o)
{
   apply_delta_balances();
   apply_delta_fee_pools();

   db().modify(db().get_global_properties(), [&o](global_property_object& p) {
      p.pending_parameters = o.new_parameters;
   });

   return object_id_type();
}

} } // bts::chain
