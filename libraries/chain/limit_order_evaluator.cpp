#include <bts/chain/limit_order_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
object_id_type limit_order_create_evaluator::do_evaluate( const limit_order_create_operation& op )
{
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

   // adjust_balance( _seller, _sell_asset, -op.amount_to_sell.amount );

   return object_id_type();
}

object_id_type limit_order_create_evaluator::do_apply( const limit_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   wdump( (seller_balance)(op) );
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         if( op.amount_to_sell.asset_id == asset_id_type() )
            bal.total_core_in_orders += op.amount_to_sell.amount;
         bal.sub_balance( op.amount_to_sell );
   });

   const auto& new_order_object = db().create<limit_order_object>( [&]( limit_order_object& obj ){
       obj.seller   = _seller->id;
       obj.for_sale = op.amount_to_sell.amount;
       obj.sell_price = op.amount_to_sell / op.min_to_receive;
   });
   auto result = new_order_object.id; // save this because we may remove the object by filling it

   const auto& order_idx = db().get_index_type<limit_order_index>();
   const auto& price_idx = order_idx.indices().get<by_price>();

   // TODO: it should be possible to simply check the NEXT/PREV iterator after new_order_object to
   // determine whether or not this order has "changed the book" in a way that requires us to
   // check orders.   For now I just lookup the lower bound and check for equality... this is log(n) vs
   // constant time check. Potential optimization.
   auto best_itr = price_idx.lower_bound( _sell_asset->amount(0) / op.min_to_receive );
   if( best_itr->id != new_order_object.id ) return new_order_object.id;

   auto max_price  = op.min_to_receive / op.amount_to_sell;
   auto itr = price_idx.lower_bound( _receive_asset->amount(0) / op.amount_to_sell );
   auto end = price_idx.end();

   while( itr != end && itr->sell_price <= max_price )
   {
      auto old_itr = itr;
      ++itr;
      if( match( new_order_object, *old_itr ) != 2 )
         break; // 2 means ONLY old iter filled
   }

   apply_delta_balances();
   apply_delta_fee_pools();

   return result;
} // limit_order_evaluator::do_apply

object_id_type limit_order_cancel_evaluator::do_evaluate( const limit_order_cancel_operation& o )
{
   database&    d = db();

   auto bts_fee_paid      = pay_fee( o.fee_paying_account, o.fee );
   auto bts_fee_required  = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _order = &o.order(d);
   FC_ASSERT( _order->seller == o.fee_paying_account  );
   FC_ASSERT( o.refunded == _order->amount_for_sale() );
   adjust_balance( fee_paying_account, &o.refunded.asset_id(d),  o.refunded.amount );

  return object_id_type();
}

object_id_type limit_order_cancel_evaluator::do_apply( const limit_order_cancel_operation& o )
{
  database&   d = db();

  apply_delta_balances();
  apply_delta_fee_pools();

  d.remove( *_order );

  if( o.refunded.asset_id == asset_id_type() )
  {
     auto& bal_obj = fee_paying_account->balances(d);
     d.modify( bal_obj, [&]( account_balance_object& obj ){
         obj.total_core_in_orders -= o.refunded.amount;
     });
  }
  return object_id_type();
}



} } // bts::chain
