#include <bts/chain/short_order_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
object_id_type short_order_create_evaluator::do_evaluate( const short_order_create_operation& op )
{
   database& d = db();

   auto bts_fee_paid = pay_fee( op.seller, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );
   //_priority_fee = bts_fee_paid - bts_fee_required;

   const asset_object& base_asset  = op.amount_to_sell.asset_id(d);    
   const asset_object& quote_asset = op.collateral.asset_id(d);    

   FC_ASSERT( base_asset.is_market_issued() );
   FC_ASSERT( quote_asset.id == base_asset.short_backing_asset );
   _seller = fee_paying_account;
   _receive_asset = &quote_asset;

   // TODO: FC_ASSERT( op.initial_collateral_ratio >= CURRENT_INIT_COLLATERAL_RATIO_REQUIREMENTS )
   // TODO: FC_ASSERT( op.maintenance_collateral_ratio >= CURRENT_INIT_COLLATERAL_RATIO_REQUIREMENTS )
   // TODO: FC_ASSERT( op.short_price() >= CURRENT_PRICE_LIMIT  )

   return object_id_type();
}

object_id_type short_order_create_evaluator::do_apply( const short_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         bal.sub_balance( op.collateral );
   });
   
   const auto& new_order_object = db().create<short_order_object>( [&]( short_order_object& obj ){
       obj.seller                      = _seller->id;
       obj.for_sale                    = op.amount_to_sell.amount;
       obj.available_collateral        = op.collateral.amount;
       obj.short_price                 = op.short_price();
       obj.call_price                  = op.call_price();
       obj.initial_collateral_ratio    = op.initial_collateral_ratio;
       obj.maitenance_collateral_ratio = op.maitenance_collateral_ratio;
   });
   auto new_id = new_order_object.id;
   
   if( op.collateral.asset_id == asset_id_type() )
   {
      auto& bal_obj = fee_paying_account->balances(db());
      db().modify( bal_obj, [&]( account_balance_object& obj ){
          obj.total_core_in_orders += op.collateral.amount;
      });
   }

   const auto& order_idx = db().get_index_type<limit_order_index>();
   const auto& price_idx = order_idx.indices().get<by_price>();
   
   // TODO if this isn't the BEST short order above the limit then we can skip
   // the rest of the evaluation.
   //auto best_itr = price_idx.lower_bound( _sell_asset->amount(0) / op.min_to_receive );
   //if( best_itr->id != new_order_object.id ) return new_order_object.id;

   auto max_price  = op.short_price();
   // TODO: limit max_price by asset properties. 
   auto itr = price_idx.lower_bound( _receive_asset->amount(0) / op.amount_to_sell );
   auto end = price_idx.end();

   while( itr != end && itr->sell_price <= max_price )
   {
      auto old_itr = itr;
      ++itr;
      if( match( *old_itr, new_order_object, old_itr->sell_price ) != 1 )
         break; // 1 means ONLY old iter filled
   }

   wlog( "." );
   apply_delta_balances();
   wlog( "." );
   apply_delta_fee_pools();
   wlog( "." );

   return new_id;
} // short_order_evaluator::do_apply


asset short_order_cancel_evaluator::do_evaluate( const short_order_cancel_operation& o )
{
   database&    d = db();

   auto bts_fee_paid      = pay_fee( o.fee_paying_account, o.fee );
   auto bts_fee_required  = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );
  
   _order = &o.order(d);
   FC_ASSERT( _order->seller == o.fee_paying_account  );
   auto refunded = _order->get_collateral();
   adjust_balance( fee_paying_account, &refunded.asset_id(d),  refunded.amount );

  return refunded;
}

asset short_order_cancel_evaluator::do_apply( const short_order_cancel_operation& o )
{
  database&   d = db();

  apply_delta_balances();
  apply_delta_fee_pools();

  auto refunded = _order->get_collateral();

  d.remove( *_order );

  if( refunded.asset_id == asset_id_type() )
  {
     auto& bal_obj = fee_paying_account->balances(d);
     d.modify( bal_obj, [&]( account_balance_object& obj ){
         obj.total_core_in_orders -= refunded.amount;
     });
  }
  return refunded;
}



} } // bts::chain
