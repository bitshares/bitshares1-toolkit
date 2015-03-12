#include <bts/chain/transfer_evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {
object_id_type transfer_evaluator::do_evaluate( const transfer_operation& op )
{
   database& d = db();

   auto bts_fee_paid = pay_fee( op.from, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   const account_object& from_account = *fee_paying_account;
   const account_object& to_account   = op.to(d);
   const asset_object&   asset_type   = op.amount.asset_id(d);

   if( asset_type.flags & white_list )
   {
      FC_ASSERT( to_account.is_authorized_asset( op.amount.asset_id ) );
      FC_ASSERT( from_account.is_authorized_asset( op.amount.asset_id ) );
   }

   // SHOULD BE HANDLED BY pay_fee FC_ASSERT( verify_authority( &from_account, authority::active ) );
   FC_ASSERT( get_balance( &from_account, &asset_type ) >= op.amount );

   adjust_balance( &from_account, &asset_type, -op.amount.amount );
   adjust_balance( &to_account, &asset_type, op.amount.amount );

   return object_id_type();
}

object_id_type transfer_evaluator::do_apply( const transfer_operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   return object_id_type();
}
} } // bts::chain
