#include <bts/chain/witness_evaluator.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type witness_create_evaluator::do_evaluate( const witness_create_operation& op )
{
   database& d = db();

   auto bts_fee_paid = pay_fee( op.witness_account, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   return object_id_type();
}

object_id_type witness_create_evaluator::do_apply( const witness_create_operation& op )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   const auto& vote_obj = db().create<vote_tally_object>( [&]( vote_tally_object& ){
         // initial vote is 0
   });

   const auto& new_witness_object = db().create<witness_object>( [&]( witness_object& obj ){
         obj.witness_account     = op.witness_account;
         obj.vote                = vote_obj.id;
         obj.signing_key         = op.block_signing_key;
         obj.next_secret         = op.initial_secret;
   });
   return new_witness_object.id;
}

object_id_type witness_withdraw_pay_evaluator::do_evaluate(const witness_withdraw_pay_evaluator::operation_type& o)
{
   database& d = db();

   witness = &d.get(o.from_witness);
   FC_ASSERT( o.to_account == witness->witness_account );
   FC_ASSERT( o.amount <= witness->accumulated_income, "Attempting to withdraw ${w}, but witness has only earned ${e}.",
              ("w", o.amount)("e", witness->accumulated_income) );
   to_account = &d.get(o.to_account);

   adjust_balance(to_account, &d.get(asset_id_type()), o.amount);

   // Defer fee payment to the end, so that if the withdrawn funds can pay the fee if necessary.
   auto bts_fee_paid = pay_fee( o.to_account, o.fee );
   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   return object_id_type();
}

object_id_type witness_withdraw_pay_evaluator::do_apply(const witness_withdraw_pay_evaluator::operation_type& o)
{
   database& d = db();

   apply_delta_balances();
   apply_delta_fee_pools();

   d.modify(*witness, [&o](witness_object& w) {
      w.accumulated_income -= o.amount;
   });

   return object_id_type();
}

} } // bts::chain
