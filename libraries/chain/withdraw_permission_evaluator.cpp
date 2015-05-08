#include <bts/chain/withdraw_permission_evaluator.hpp>
#include <bts/chain/withdraw_permission_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

object_id_type withdraw_permission_create_evaluator::do_evaluate(const operation_type& op)
{
   database& d = db();

   FC_ASSERT(d.find_object(op.withdraw_from_account));
   FC_ASSERT(d.find_object(op.authorized_account));
   FC_ASSERT(d.find_object(op.withdrawal_limit.asset_id));
   FC_ASSERT(op.period_start_time >= d.head_block_time());
   FC_ASSERT(op.period_start_time + op.periods_until_expiration * op.withdrawal_period_sec > d.head_block_time());
   FC_ASSERT(op.withdrawal_period_sec >= d.get_global_properties().parameters.block_interval);

   return d.get_index_type<withdraw_permission_index>().get_next_id();
}

object_id_type withdraw_permission_create_evaluator::do_apply(const operation_type& op)
{
   return db().create<withdraw_permission_object>([&op](withdraw_permission_object& p) {
      p.withdraw_from_account = op.withdraw_from_account;
      p.authorized_account = op.authorized_account;
      p.withdrawal_limit = op.withdrawal_limit;
      p.withdrawal_period_sec = op.withdrawal_period_sec;
      p.remaining_periods = op.periods_until_expiration;
      p.next_period_start_time = op.period_start_time;
   }).id;
}

object_id_type withdraw_permission_claim_evaluator::do_evaluate(const withdraw_permission_claim_evaluator::operation_type& op)
{
   database& d = db();

   const withdraw_permission_object& permit = op.withdraw_permission(d);
   FC_ASSERT(permit.authorized_account == op.withdraw_to_account);
   FC_ASSERT(permit.withdraw_from_account == op.withdraw_from_account);
   FC_ASSERT(permit.next_period_start_time <= d.head_block_time());
   FC_ASSERT(op.amount_to_withdraw <= permit.withdrawal_limit);
   FC_ASSERT(fc::safe<uint64_t>(permit.next_period_start_time.sec_since_epoch()) +
             uint64_t(permit.withdrawal_period_sec) * permit.remaining_periods > d.head_block_time().sec_since_epoch());
   FC_ASSERT(d.get_balance(op.withdraw_from_account, op.amount_to_withdraw.asset_id) >= op.amount_to_withdraw);

   return object_id_type();
}

object_id_type withdraw_permission_claim_evaluator::do_apply(const withdraw_permission_claim_evaluator::operation_type& op)
{
   database& d = db();

   const withdraw_permission_object& permit = d.get(op.withdraw_permission);
   d.modify(permit, [&](withdraw_permission_object& p) {
      do {
         p.next_period_start_time += p.withdrawal_period_sec;
         --p.remaining_periods;
      }
      while( p.remaining_periods > 0 && p.next_period_start_time <= d.head_block_time() );
   });
   if( permit.remaining_periods == 0 )
      d.remove(permit);

   d.adjust_balance(op.withdraw_from_account, -op.amount_to_withdraw);
   d.adjust_balance(op.withdraw_to_account, op.amount_to_withdraw);

   return object_id_type();
}

} } // bts::chain
