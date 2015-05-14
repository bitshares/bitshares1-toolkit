#include <bts/chain/witness_evaluator.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type witness_create_evaluator::do_evaluate( const witness_create_operation& op )
{
   FC_ASSERT(db().get(op.witness_account).is_prime());
   return object_id_type();
}

object_id_type witness_create_evaluator::do_apply( const witness_create_operation& op )
{
   vote_id_type vote_id;
   db().modify(db().get_global_properties(), [&vote_id](global_property_object& p) {
      vote_id = p.get_next_vote_id(vote_id_type::witness);
   });

   const auto& new_witness_object = db().create<witness_object>( [&]( witness_object& obj ){
         obj.witness_account     = op.witness_account;
         obj.vote_id             = vote_id;
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

   return object_id_type();
}

object_id_type witness_withdraw_pay_evaluator::do_apply(const witness_withdraw_pay_evaluator::operation_type& o)
{
   database& d = db();

   d.adjust_balance(o.to_account, asset(o.amount));

   d.modify(*witness, [&o](witness_object& w) {
      w.accumulated_income -= o.amount;
   });

   return object_id_type();
}

} } // bts::chain
