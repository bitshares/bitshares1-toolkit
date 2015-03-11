#include <bts/chain/limit_order_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/limit_order_object.hpp>

namespace bts { namespace chain {
object_id_type limit_order_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<limit_order_create_operation>();
   _op = &op;

   database& d = db();

   auto bts_fee_paid = pay_fee( op.seller, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _seller        = this->fee_paying_account;
   _sell_asset    = &op.amount_to_sell.asset_id(d);
   _receive_asset = &op.min_to_receive.asset_id(d);


   if( _sell_asset->flags & white_list ) FC_ASSERT( _seller->is_authorized_asset( _sell_asset->id ) );
   if( _receive_asset->flags & white_list ) FC_ASSERT( _seller->is_authorized_asset( _receive_asset->id ) );

   FC_ASSERT( get_balance( _seller, _sell_asset ) >= op.amount_to_sell );

   adjust_balance( _seller, _sell_asset, -op.amount_to_sell.amount );

   return object_id_type();
}

object_id_type limit_order_evaluator::apply( const operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   const auto& seller_balance = _seller->balances(db());
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         bal.sub_balance( -_op->amount_to_sell );
   });

   const auto& new_order_object = db().create<limit_order_object>( [&]( limit_order_object& obj ){
       obj.seller   = _seller->id;
       obj.for_sale = _op->amount_to_sell.amount;
       obj.sell_price = _op->min_to_receive / _op->amount_to_sell;
   });

   return new_order_object.id;
}
} } // bts::chain
