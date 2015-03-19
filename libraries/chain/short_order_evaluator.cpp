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
   _sell_asset    = &base_asset;

   // TODO: FC_ASSERT( op.initial_collateral_ratio >= CURRENT_INIT_COLLATERAL_RATIO_REQUIREMENTS )
   // TODO: FC_ASSERT( op.maintenance_collateral_ratio >= CURRENT_INIT_COLLATERAL_RATIO_REQUIREMENTS )
   // TODO: FC_ASSERT( op.sell_price() >= CURRENT_PRICE_LIMIT  )

   return object_id_type();
}

object_id_type short_order_create_evaluator::do_apply( const short_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         bal.sub_balance( op.collateral );
   });
   
   const auto& new_order_object = db().create<short_order_object>( [&]( short_order_object& obj ){
       obj.seller                       = _seller->id;
       obj.for_sale                     = op.amount_to_sell.amount;
       obj.available_collateral         = op.collateral.amount;
       obj.sell_price                  = op.sell_price();
       obj.call_price                   = op.call_price();
       obj.initial_collateral_ratio     = op.initial_collateral_ratio;
       obj.maintenance_collateral_ratio = op.maintenance_collateral_ratio;
   });
   short_order_id_type new_id = new_order_object.id;

   if( op.collateral.asset_id == asset_id_type() )
   {
      auto& bal_obj = fee_paying_account->balances(db());
      db().modify( bal_obj, [&]( account_balance_object& obj ){
          obj.total_core_in_orders += op.collateral.amount;
      });
   }

   check_call_orders(*_sell_asset);

   if( !db().find(new_id) ) // then we were filled by call order
   {
      apply_delta_balances();
      apply_delta_fee_pools();
      return new_id;
   }
   

   const auto& limit_order_idx = db().get_index_type<limit_order_index>();
   const auto& limit_price_idx = limit_order_idx.indices().get<by_price>();

   //wdump( (op.sell_price().to_real()) );
   auto min_limit_price  = ~op.sell_price();
   //wdump( (min_limit_price.to_real()) );

   //auto itr = limit_price_idx.lower_bound( min_limit_price );
   //auto end = limit_price_idx.upper_bound( min_limit_price.max() );
   auto itr = limit_price_idx.lower_bound( min_limit_price.max() );
   auto end = limit_price_idx.upper_bound( min_limit_price );

   while( itr != end )
   {
      //wdump( (itr->sell_price)(max_price) );
      //wdump( (itr->sell_price.to_real())(max_price.to_real()) );
      auto old_itr = itr;
      ++itr;
      if( match( *old_itr, new_order_object, old_itr->sell_price ) != 1 )
         break; // 1 means ONLY old iter filled
   }

   apply_delta_balances();
   apply_delta_fee_pools();

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
