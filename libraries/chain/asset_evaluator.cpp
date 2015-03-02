#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/asset_object.hpp>

namespace bts { namespace chain {
object_id_type asset_create_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<asset_create_operation>();
   database& d = db();

   /*
   auto bts_fee_paid = pay_fee( op.from, op.fee );
   auto bts_fee_required = d.current_fee( asset_create_fee_type );
   //bts_fee_required += share_type((op.memo.size() * d.current_fee( data_fee_type ).value)/1024);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   const account_object* from_account = fee_paying_account;
   const account_object* to_account   = op.to(d);
   const asset_object*   asset_type   = op.amount.asset_id(d);
   asset( asset_type == fee_asset );

   FC_ASSERT( to_account->is_authorized_asset( op.amount.asset_id ) );
   FC_ASSERT( from_account->is_authorized_asset( op.amount.asset_id ) );

   FC_ASSERT( verify_authority( from_account, authority::active ) );
   FC_ASSERT( get_balance( from_account, asset_type ) >= op.amount );

   adjust_balance( from_account, asset_type, -op.amount.amount );
   adjust_balance( to_account, asset_type, op.amount.amount );
   */

   return object_id_type();
}

object_id_type asset_create_evaluator::apply( const operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   return object_id_type();
}
} } // bts::chain
