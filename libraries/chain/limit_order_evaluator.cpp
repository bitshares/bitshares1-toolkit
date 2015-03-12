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

   adjust_balance( _seller, _sell_asset, -op.amount_to_sell.amount );

   return object_id_type();
}

object_id_type limit_order_create_evaluator::do_apply( const limit_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         bal.sub_balance( -op.amount_to_sell );
   });

   const auto& new_order_object = db().create<limit_order_object>( [&]( limit_order_object& obj ){
       obj.seller   = _seller->id;
       obj.for_sale = op.amount_to_sell.amount;
       obj.sell_price = op.amount_to_sell / op.min_to_receive;
   });


  if( op.amount_to_sell.asset_id == asset_id_type() )
  {
     auto& bal_obj = fee_paying_account->balances(db());
     db().modify( bal_obj, [&]( account_balance_object& obj ){
         obj.total_core_in_orders += op.amount_to_sell.amount;
     });
  }


   const auto& order_idx = db().get_index_type<limit_order_index>();

   const auto& price_idx = order_idx.indices().get<by_price>();
   auto max_price  = op.min_to_receive / op.amount_to_sell;
   auto itr = price_idx.lower_bound( asset(0,op.min_to_receive.asset_id) / op.amount_to_sell );
   auto end = price_idx.end();

   asset total_sell_issuer_fees = asset( 0, _sell_asset->id    );
   asset total_recv_issuer_fees = asset( 0, _receive_asset->id );

   auto post_eval_order = new_order_object;

   while( itr != end && itr->sell_price <= max_price )
   {
      auto match_price = itr->sell_price;
      auto itr_for_sale = itr->amount_for_sale();
      auto obj_for_sale = post_eval_order.amount_for_sale();
      auto max_itr_pays = itr_for_sale*match_price;
      auto max_obj_pays = obj_for_sale*match_price;
      auto match_size  = std::min( max_itr_pays, max_obj_pays );

      asset itr_receives;
      asset itr_pays;
      asset obj_receives;
      asset obj_pays;

      /* to handle rounding issues, we know that one order or the
       * other MUST be filled.
       */
      if( match_size == max_itr_pays )
      {
         itr_pays     = itr_for_sale;
         itr_receives = itr_for_sale * match_price;
         obj_receives = itr_pays;
         obj_pays     = itr_receives;
      }
      else
      {
         obj_pays     = obj_for_sale;
         obj_receives = obj_for_sale * match_price;
         itr_pays     = obj_receives;
         itr_receives = obj_pays;
      }
      FC_ASSERT( itr_pays <= itr_for_sale );
      FC_ASSERT( obj_pays <= obj_for_sale );

      auto sell_issuer_fees     = calculate_market_fee( _sell_asset, obj_pays );
      auto receive_issuer_fees  = calculate_market_fee( _receive_asset, obj_receives  );

      obj_receives -= receive_issuer_fees;
      itr_receives -= sell_issuer_fees;

      const account_object& receiver = itr->seller(db());
      adjust_balance( _seller, _receive_asset, obj_receives.amount );
      adjust_balance( &receiver, _sell_asset, itr_receives.amount );

      if( _sell_asset->id.instance() == 0 ) /* core asset vote update */
         adjust_votes( _seller->delegate_votes, -obj_pays.amount );
      if( _receive_asset->id.instance() == 0 ) /* core asset vote update */
         adjust_votes( receiver.delegate_votes, -itr_pays.amount );

      if( itr_pays.amount == itr->for_sale )
      {
         auto old_itr = itr;
         ++itr;
         db().remove( *old_itr );
      }
      else
      {
         db().modify( *itr, [&]( limit_order_object& itr_obj ){
             itr_obj.for_sale -= itr_pays.amount;
         });
         ++itr;
      }

      total_sell_issuer_fees += sell_issuer_fees;
      total_recv_issuer_fees += receive_issuer_fees;

      post_eval_order.for_sale -= obj_pays.amount;
      FC_ASSERT( post_eval_order.for_sale.value > 0 );
      if( post_eval_order.for_sale == 0 )
      {
         // remove the object.
         db().remove( new_order_object );
         break;
      }
      // ITR is incremented when we decide to either modify or remove it.
   }

   if( post_eval_order.for_sale != new_order_object.for_sale )
   {
      db().modify( new_order_object, [&]( limit_order_object& obj ){
                   obj.for_sale = post_eval_order.for_sale;
                   });
   }

   const auto& sell_asset_dyn_data = _sell_asset->dynamic_asset_data_id(db());
   db().modify( sell_asset_dyn_data, [&]( asset_dynamic_data_object& obj ){
                obj.accumulated_fees += total_sell_issuer_fees.amount;
                });

   const auto& recv_asset_dyn_data = _receive_asset->dynamic_asset_data_id(db());
   db().modify( recv_asset_dyn_data, [&]( asset_dynamic_data_object& obj ){
                obj.accumulated_fees += total_recv_issuer_fees.amount;
                });


   apply_delta_balances();
   apply_delta_fee_pools();

   return new_order_object.id;
} // limit_order_evaluator::do_apply

asset limit_order_create_evaluator::calculate_market_fee( const asset_object* aobj, const asset& trade_amount )
{
   fc::uint128 a(trade_amount.amount.value);
   a *= aobj->market_fee_percent;
   a /= BTS_MAX_MARKET_FEE_PERCENT;
   return asset( a.to_uint64(), trade_amount.asset_id );
}


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
