#include <bts/chain/limit_order_evaluator.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
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

   FC_ASSERT( get_balance( _seller, _sell_asset ) >= op.amount_to_sell, "insufficient balance", ("balance",get_balance(_seller,_sell_asset))("amount_to_sell",op.amount_to_sell) );

   // adjust_balance( _seller, _sell_asset, -op.amount_to_sell.amount );

   return object_id_type();
}
template<typename I>
std::reverse_iterator<I> reverse( const I& itr ) { return std::reverse_iterator<I>(itr); }

object_id_type limit_order_create_evaluator::do_apply( const limit_order_create_operation& op )
{
   const auto& seller_balance = _seller->balances(db());
   //wdump( (seller_balance)(op) );
   db().modify( seller_balance, [&]( account_balance_object& bal ){
         if( op.amount_to_sell.asset_id == asset_id_type() )
         {
            bal.total_core_in_orders += op.amount_to_sell.amount;
         }
         bal.sub_balance( op.amount_to_sell );
   });

   const auto& new_order_object = db().create<limit_order_object>( [&]( limit_order_object& obj ){
       obj.seller   = _seller->id;
       obj.for_sale = op.amount_to_sell.amount;
       obj.sell_price = op.get_price();
   });
   limit_order_id_type result = new_order_object.id; // save this because we may remove the object by filling it

   bool called_some = check_call_orders(*_sell_asset);
   called_some |= check_call_orders(*_receive_asset);
   if( called_some && !db().find(result) ) // then we were filled by call order
   {
      apply_delta_balances();
      apply_delta_fee_pools();
      return result;
   }

   const auto& limit_order_idx = db().get_index_type<limit_order_index>();
   const auto& limit_price_idx = limit_order_idx.indices().get<by_price>();

   // TODO: it should be possible to simply check the NEXT/PREV iterator after new_order_object to
   // determine whether or not this order has "changed the book" in a way that requires us to
   // check orders.   For now I just lookup the lower bound and check for equality... this is log(n) vs
   // constant time check. Potential optimization.

   //auto best_limit_itr = limit_price_idx.lower_bound( _sell_asset->amount(0) / op.min_to_receive );
   //if( best_limit_itr->id != new_order_object.id ) return new_order_object.id;


   auto max_price  = ~op.get_price(); //op.min_to_receive / op.amount_to_sell;
   //auto limit_end = reverse(limit_price_idx.lower_bound( max_price ));
   //auto limit_itr = reverse(limit_price_idx.upper_bound( max_price.max() ));
   auto limit_itr = limit_price_idx.lower_bound( max_price.max() );
   auto limit_end = limit_price_idx.upper_bound( max_price );
   for( auto itr = limit_price_idx.begin(); itr != limit_price_idx.end(); ++itr )
   {
      idump((*itr));
      if( itr == limit_itr ) wdump((*limit_itr));
      if( itr == limit_end ) edump((*limit_end));
   }

   for( auto tmp = limit_itr; tmp != limit_end; ++tmp )
   {
      assert( tmp != limit_price_idx.end() );
      wdump((*tmp));
   }

   bool filled = false;
   //if( new_order_object.amount_to_receive().asset_id(db()).is_market_issued() )
   if( _receive_asset->is_market_issued() )
   { // then we may also match against shorts
      if( _receive_asset->short_backing_asset == asset_id_type() )
      {
         bool converted_some = convert_fees( *_receive_asset );
         // just incase the new order was completely filled from fees
         if( converted_some && !db().find(result) ) // then we were filled by call order
         {
            apply_delta_balances();
            apply_delta_fee_pools();
            return result;
         }
      }
      const auto& short_order_idx = db().get_index_type<short_order_index>();
      const auto& sell_price_idx = short_order_idx.indices().get<by_price>();

      FC_ASSERT( max_price.max() >= max_price );
      //auto short_itr = reverse(sell_price_idx.upper_bound( max_price.max() ));
      //auto short_end = reverse(sell_price_idx.lower_bound( max_price ));
      auto short_itr = sell_price_idx.lower_bound( max_price.max() );
      auto short_end = sell_price_idx.upper_bound( max_price );

      while( !filled )
      {
         if( limit_itr != limit_end )
         {
            if( short_itr != short_end && limit_itr->sell_price < short_itr->sell_price )
            {
               ilog( "next short with limits" );
               auto old_short_itr = short_itr;
               ++short_itr;
               filled = (2 != match( new_order_object, *old_short_itr, old_short_itr->sell_price ) );
            }
            else 
            {
               ilog( "next limit" );
               auto old_limit_itr = limit_itr;
               ++limit_itr;
               filled = (2 != match( new_order_object, *old_limit_itr, old_limit_itr->sell_price ) );
            }
         } 
         else if( short_itr != short_end  )
         {
            wlog( "next short no limits left" );
            auto old_short_itr = short_itr;
            ++short_itr;
            filled = (2 != match( new_order_object, *old_short_itr, old_short_itr->sell_price ) );
         }
         else break;
      }
   }
   else while( !filled && limit_itr != limit_end  )
   {
         auto old_itr = limit_itr;
         ++limit_itr;
         filled = (2 != match( new_order_object, *old_itr, old_itr->sell_price ));
   }

   apply_delta_balances();
   apply_delta_fee_pools();

   return result;
} // limit_order_evaluator::do_apply

asset limit_order_cancel_evaluator::do_evaluate( const limit_order_cancel_operation& o )
{
   database&    d = db();

   auto bts_fee_paid      = pay_fee( o.fee_paying_account, o.fee );
   auto bts_fee_required  = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _order = &o.order(d);
   FC_ASSERT( _order->seller == o.fee_paying_account  );
   auto refunded = _order->amount_for_sale();
   adjust_balance( fee_paying_account, &refunded.asset_id(d),  refunded.amount );

  return refunded;
}

asset limit_order_cancel_evaluator::do_apply( const limit_order_cancel_operation& o )
{
  database&   d = db();

  apply_delta_balances();
  apply_delta_fee_pools();

  auto refunded = _order->amount_for_sale();

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
