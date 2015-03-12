#include <bts/chain/short_order_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
object_id_type short_order_create_evaluator::do_evaluate( const short_order_create_operation& op )
{
   database& d = db();

   auto bts_fee_paid = pay_fee( op.seller, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );
   _priority_fee = bts_fee_paid - bts_fee_required;

   const asset_object& base_asset  = op.short_price.base.asset_id(d);    
   const asset_object& quote_asset = op.short_price.quote.asset_id(d);    

   FC_ASSERT( base_asset.is_market_issued() || quote_asset.is_market_issued() );
   if( base_asset.is_market_issued() ) FC_ASSERT( object_id_type(base_asset.short_backing_asset) == quote_asset.id );
   if( quote_asset.is_market_issued() ) FC_ASSERT( object_id_type(quote_asset.short_backing_asset) == base_asset.id );

   return object_id_type();
}

object_id_type short_order_create_evaluator::do_apply( const short_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         bal.sub_balance( -op.collateral );
   });

   const auto& new_order_object = db().create<short_order_object>( [&]( short_order_object& obj ){
       obj.seller                      = _seller->id;
       obj.available_collateral        = op.collateral.amount;
       obj.short_price                 = op.short_price;
       obj.priority_fee                = _priority_fee;
       obj.initial_collateral_ratio    = op.initial_collateral_ratio;
       obj.maitenance_collateral_ratio = op.maitenance_collateral_ratio;
   });

   apply_delta_balances();
   apply_delta_fee_pools();

   return new_order_object.id;
} // short_order_evaluator::do_apply


object_id_type short_order_cancel_evaluator::do_evaluate( const short_order_cancel_operation& o )
{
   database&    d = db();

   auto bts_fee_paid      = pay_fee( o.fee_paying_account, o.fee );
   auto bts_fee_required  = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );
  
   _order = &o.order(d);
   FC_ASSERT( _order->seller == o.fee_paying_account  );
   FC_ASSERT( o.refunded == _order->get_collateral() );
   adjust_balance( fee_paying_account, &o.refunded.asset_id(d),  o.refunded.amount );

  return object_id_type();
}

object_id_type short_order_cancel_evaluator::do_apply( const short_order_cancel_operation& o )
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
