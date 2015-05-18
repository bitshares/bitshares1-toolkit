
#include <bts/chain/account_object.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/vesting_balance_evaluator.hpp>
#include <bts/chain/vesting_balance_object.hpp>

namespace bts { namespace chain {

object_id_type vesting_balance_create_evaluator::do_evaluate( const vesting_balance_create_operation& op )
{
   const database& d = db();

   const account_object& creator_account = op.creator( d );
   /* const account_object& owner_account = */ op.owner( d );

   // TODO: Check asset authorizations and withdrawals

   FC_ASSERT( op.amount.amount > 0 );
   FC_ASSERT( d.get_balance( creator_account.id, op.amount.asset_id ) >= op.amount );

   return object_id_type();
}

object_id_type vesting_balance_create_evaluator::do_apply( const vesting_balance_create_operation& op )
{
   database& d = db();
   const time_point_sec now = d.head_block_time();

   d.adjust_balance( op.creator, -op.amount );

   const vesting_balance_object& vbo = d.create< vesting_balance_object >( [&]( vesting_balance_object& obj )
   {
      //WARNING: The logic to create a vesting balance object is replicated in vesting_balance_worker_type::initializer::init.
      // If making changes to this logic, check if those changes should also be made there as well.
      obj.owner = op.owner;
      obj.balance = op.amount;

      cdd_vesting_policy policy;
      policy.vesting_seconds = op.vesting_seconds;
      policy.coin_seconds_earned = 0;
      policy.coin_seconds_earned_last_update = now;

      obj.policy = policy;
   } );

   FC_ASSERT( d.get_balance( op.creator, op.amount.asset_id ) >= op.amount );

   return vbo.id;
}

object_id_type vesting_balance_withdraw_evaluator::do_evaluate( const vesting_balance_withdraw_operation& op )
{
   const database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );
   FC_ASSERT( op.owner == vbo.owner );
   FC_ASSERT( vbo.is_withdraw_allowed( now, op.amount ) );
   assert( op.amount <= vbo.balance );      // is_withdraw_allowed should fail before this check is reached

   /* const account_object& owner_account = */ op.owner( d );
   // TODO: Check asset authorizations and withdrawals
   return object_id_type();
}

object_id_type vesting_balance_withdraw_evaluator::do_apply( const vesting_balance_withdraw_operation& op )
{
   database& d = db();
   const time_point_sec now = d.head_block_time();

   const vesting_balance_object& vbo = op.vesting_balance( d );

   // Allow zero balance objects to stick around, (1) to comply
   // with the chain's "objects live forever" design principle, (2)
   // if it's cashback or worker, it'll be filled up again.

   d.modify( vbo, [&]( vesting_balance_object& vbo )
   {
      vbo.withdraw( now, op.amount );
   } );

   d.adjust_balance( op.owner, op.amount );

   // TODO: Check asset authorizations and withdrawals
   return object_id_type();
}

} } // bts::chain
