#include <bts/chain/transfer_evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {
object_id_type transfer_evaluator::do_evaluate( const transfer_operation& op )
{ try {
   database& d = db();

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   asset_type      = op.amount.asset_id(d);
   const asset_object&   fee_asset_type  = op.fee.asset_id(d);

   if( asset_type.flags & white_list )
   {
      FC_ASSERT( to_account.is_authorized_asset( asset_type ) );
      FC_ASSERT( from_account.is_authorized_asset( asset_type ) );
   }

   if( fee_asset_type.flags & white_list )
      FC_ASSERT( from_account.is_authorized_asset( asset_type ) );

   auto bts_fee_paid = pay_fee( op.from, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   FC_ASSERT( get_balance( &from_account, &asset_type ).amount >= op.amount.amount, 
              "", ("total_transfer",op.amount)("balance",get_balance(&from_account, &asset_type).amount) );

   adjust_balance( &from_account, &asset_type, -op.amount.amount );
   adjust_balance( &to_account, &asset_type, op.amount.amount );

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type transfer_evaluator::do_apply( const transfer_operation& o )
{ try {
   apply_delta_balances();
   apply_delta_fee_pools();

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) )}
} } // bts::chain
