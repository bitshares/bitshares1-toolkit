#include <bts/chain/database.hpp>
#include <bts/chain/evaluator.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>

#include <fc/uint128.hpp>

namespace bts { namespace chain {
   database& generic_evaluator::db()const { return trx_state->db(); }
   operation_result generic_evaluator::start_evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply )
   {
      trx_state   = &eval_state;
      check_required_authorities(op);
      auto result = evaluate( op );

      if( apply ) result = this->apply( op );
      return result;
   }

   void generic_evaluator::prepare_fee(account_id_type account_id, asset fee)
   {
      fee_from_account = fee;
      FC_ASSERT( fee.amount >= 0 );
      fee_paying_account = &account_id(db());
      fee_paying_account_statistics = &fee_paying_account->statistics(db());

      fee_asset = &fee.asset_id(db());
      fee_asset_dyn_data = &fee_asset->dynamic_asset_data_id(db());

      if( fee_from_account.asset_id == asset_id_type() )
         core_fee_paid = fee_from_account.amount;
      else {
         asset fee_from_pool = fee_from_account * fee_asset->core_exchange_rate;
         FC_ASSERT( fee_from_pool.asset_id == asset_id_type() );
         core_fee_paid = fee_from_pool.amount;
         FC_ASSERT( core_fee_paid <= fee_asset_dyn_data->fee_pool );
      }
   }

   void generic_evaluator::pay_fee()
   { try {
      asset core_fee_subtotal(core_fee_paid);
      const auto& gp = db().get_global_properties();
      auto bulk_cashback  = share_type(0);
      if( fee_paying_account_statistics->lifetime_fees_paid > gp.parameters.bulk_discount_threshold_min &&
          fee_paying_account->is_prime() )
      {
         uint64_t bulk_discount_percent = 0;
         if( fee_paying_account_statistics->lifetime_fees_paid > gp.parameters.bulk_discount_threshold_max )
            bulk_discount_percent = gp.parameters.max_bulk_discount_percent_of_fee;
         else if(gp.parameters.bulk_discount_threshold_max.value - gp.parameters.bulk_discount_threshold_min.value != 0)
         {
            bulk_discount_percent =
                  (gp.parameters.max_bulk_discount_percent_of_fee *
                            (fee_paying_account_statistics->lifetime_fees_paid.value -
                             gp.parameters.bulk_discount_threshold_min.value)) /
                  (gp.parameters.bulk_discount_threshold_max.value - gp.parameters.bulk_discount_threshold_min.value);
         }
         assert( bulk_discount_percent <= 10000 );
         assert( bulk_discount_percent >= 0 );

         bulk_cashback = (core_fee_subtotal.amount.value * bulk_discount_percent) / 10000;
         assert( bulk_cashback <= core_fee_subtotal.amount );
      }

      auto core_fee_total = core_fee_subtotal.amount - bulk_cashback;
      auto accumulated = (core_fee_total.value  * gp.parameters.witness_percent_of_fee)/10000;
      auto burned     = (core_fee_total.value  * gp.parameters.burn_percent_of_fee)/10000;
      auto referral   = core_fee_total.value - accumulated - burned;
      auto& d = db();
      auto now = d.head_block_time();

      assert( accumulated + burned <= core_fee_total );

      if( fee_asset->get_id() != asset_id_type() )
         d.modify(*fee_asset_dyn_data, [this](asset_dynamic_data_object& d) {
            d.accumulated_fees += fee_from_account.amount;
            d.fee_pool -= core_fee_paid;
         });
      d.modify(dynamic_asset_data_id_type()(d), [burned,accumulated](asset_dynamic_data_object& d) {
         d.accumulated_fees += accumulated + burned;
      });
      d.modify(fee_paying_account->referrer(d).statistics(d), [referral,now](account_statistics_object& s) {
         s.adjust_cashback(referral, now, now);
      });
      d.modify(fee_paying_account->statistics(d), [bulk_cashback,core_fee_total,now](account_statistics_object& s) {
         s.lifetime_fees_paid += core_fee_total;
         s.adjust_cashback(bulk_cashback, now, now);
      });

      assert( referral + bulk_cashback + accumulated + burned == core_fee_subtotal.amount );
   } FC_CAPTURE_AND_RETHROW() }

   bool generic_evaluator::verify_authority( const account_object* a, authority::classification c )
   {
       return trx_state->check_authority( a, c );
   }
   void generic_evaluator::check_required_authorities(const operation& op)
   {
      flat_set<account_id_type> active_auths;
      flat_set<account_id_type> owner_auths;
      op.visit(operation_get_required_auths(active_auths, owner_auths));
      for( auto id : active_auths )
         FC_ASSERT(verify_authority(&id(db()), authority::active) ||
                   verify_authority(&id(db()), authority::owner), "", ("id", id));
      for( auto id : owner_auths )
         FC_ASSERT(verify_authority(&id(db()), authority::owner), "", ("id", id));
   }

   bool generic_evaluator::verify_signature( const key_object* k )
   {
      FC_ASSERT( k != nullptr );
      return trx_state->_skip_signature_check || trx_state->signed_by.find( k->key_address() ) != trx_state->signed_by.end();
   }

   object_id_type generic_evaluator::get_relative_id( object_id_type rel_id )const
   {
      if( rel_id.space() == relative_protocol_ids )
      {
         FC_ASSERT( rel_id.instance() < trx_state->operation_results.size() );
         // fetch the object just to make sure it exists.
         auto r = trx_state->operation_results[rel_id.instance()].get<object_id_type>();
         db().get_object( r ); // make sure it exists.
         return r;
      }
      return rel_id;
   }

   authority generic_evaluator::resolve_relative_ids( const authority& a )const
   {
      authority result;
      result.auths.reserve( a.auths.size() );
      result.weight_threshold = a.weight_threshold;

      for( const auto& item : a.auths )
      {
          auto id = get_relative_id( item.first );
          FC_ASSERT( id.type() == key_object_type || id.type() == account_object_type );
          result.auths[id] = item.second;
      }

      return result;
   }

/**
 *  Matches the two orders,
 *
 *  @return a bit field indicating which orders were filled (and thus removed)
 *
 *  0 - no orders were matched
 *  1 - bid was filled
 *  2 - ask was filled
 *  3 - both were filled
 */
template<typename OrderType>
int generic_evaluator::match( const limit_order_object& usd, const OrderType& core, const price& match_price )
{
   assert( usd.sell_price.quote.asset_id == core.sell_price.base.asset_id );
   assert( usd.sell_price.base.asset_id  == core.sell_price.quote.asset_id );
   assert( usd.for_sale > 0 && core.for_sale > 0 );

   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();

   asset usd_pays, usd_receives, core_pays, core_receives;

   if( usd_for_sale <= core_for_sale * match_price )
   {
      core_receives = usd_for_sale;
      usd_receives  = usd_for_sale * match_price;
   }
   else
   {
      //This line once read: assert( core_for_sale < usd_for_sale * match_price );
      //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
      //Although usd_for_sale is greater than core_for_sale * match_price, core_for_sale == usd_for_sale * match_price
      //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
      usd_receives = core_for_sale;
      core_receives = core_for_sale * match_price;
   }

   core_pays = usd_receives;
   usd_pays  = core_receives;

   assert( usd_pays == usd.amount_for_sale() ||
           core_pays == core.amount_for_sale() );

   int result = 0;
   result |= fill_order( usd, usd_pays, usd_receives );
   result |= fill_order( core, core_pays, core_receives ) << 1;
   assert( result != 0 );
   return result;
}

int generic_evaluator::match( const call_order_object& call, const force_settlement_object& settle, const price& match_price )
{
   assert(call.get_debt().asset_id == settle.balance.asset_id );
   assert(call.debt > 0 && call.collateral > 0 && settle.balance.amount > 0);

   auto settle_for_sale = settle.balance;
   auto call_for_sale = call.collateral;

   asset call_pays, call_receives, settle_pays, settle_receives;

   if( settle_for_sale <= call_for_sale * match_price )
   {
      // We are finishing off the settle
      call_receives = settle_for_sale;
      settle_receives = settle_for_sale * match_price;
   } else {
      // We are finishing off the call
      settle_receives = call_for_sale;
      call_receives = call_for_sale * match_price;
   }

   call_pays = settle_receives;
   settle_pays = call_receives;

   assert( settle_pays == settle.balance || call_pays == call.get_collateral() );

   int result = 0;
   result |= fill_order(call, call_pays, call_receives);
   result |= fill_order(settle, settle_pays, settle_receives) << 1;

   assert(result != 0);
   return result;
}

int generic_evaluator::match( const limit_order_object& bid, const limit_order_object& ask, const price& match_price )
{
   return match<limit_order_object>( bid, ask, match_price );
}
int generic_evaluator::match( const limit_order_object& bid, const short_order_object& ask, const price& match_price )
{
   return match<short_order_object>( bid, ask, match_price );
}

/**
 *
 */
bool generic_evaluator::check_call_orders( const asset_object& mia )
{ try {
    if( !mia.is_market_issued() ) return false;
    if( mia.current_feed.call_limit.is_null() ) return false;

    const call_order_index& call_index = db().get_index_type<call_order_index>();
    const auto& call_price_index = call_index.indices().get<by_price>();

    const limit_order_index& limit_index = db().get_index_type<limit_order_index>();
    const auto& limit_price_index = limit_index.indices().get<by_price>();

    const short_order_index& short_index = db().get_index_type<short_order_index>();
    const auto& short_price_index = short_index.indices().get<by_price>();

    auto short_itr = short_price_index.lower_bound( price::max( mia.id, mia.short_backing_asset ) );
    auto short_end = short_price_index.upper_bound( ~mia.current_feed.call_limit );
    for( auto s = short_itr; s != short_end; ++s ) wdump((s->sell_price.to_real())(s->sell_price)(mia.current_feed.call_limit));

    auto limit_itr = limit_price_index.lower_bound( price::max( mia.id, mia.short_backing_asset ) );
    auto limit_end = limit_price_index.upper_bound( ~mia.current_feed.call_limit );

    auto call_itr = call_price_index.lower_bound( price::min( mia.short_backing_asset, mia.id ) );
    auto call_end = call_price_index.upper_bound( price::max( mia.short_backing_asset, mia.id ) );

    bool filled_short_or_limit = false;

    while( call_itr != call_end )
    {
       bool  current_is_limit = true;
       bool  filled_call      = false;
       price match_price;
       asset usd_for_sale;
       if( limit_itr != limit_end )
       {
          assert( limit_itr != limit_price_index.end() );
          if( short_itr != short_end && limit_itr->sell_price < short_itr->sell_price )
          {
             assert( short_itr != short_price_index.end() );
             current_is_limit = false;
             match_price      = short_itr->sell_price;
             usd_for_sale     = short_itr->amount_for_sale();
          }
          else
          {
             current_is_limit = true;
             match_price      = limit_itr->sell_price;
             usd_for_sale     = limit_itr->amount_for_sale();
          }
       }
       else if( short_itr != short_end )
       {
          assert( short_itr != short_price_index.end() );
          current_is_limit = false;
          match_price      = short_itr->sell_price;
          usd_for_sale     = short_itr->amount_for_sale();
       }
       else return filled_short_or_limit;

       match_price.validate();

       if( match_price > ~call_itr->call_price )
       {
          return filled_short_or_limit;
       }

       auto usd_to_buy   = call_itr->get_debt();

       if( usd_to_buy * match_price > call_itr->get_collateral() )
       {
          elog( "black swan, we do not have enough collateral to cover at this price" );
          settle_black_swan( mia, call_itr->get_debt() / call_itr->get_collateral() );
          return true;
       }

       asset call_pays, call_receives, order_pays, order_receives;
       if( usd_to_buy >= usd_for_sale )
       {  // fill order
          call_receives   = usd_for_sale;
          order_receives  = usd_for_sale * match_price;
          call_pays       = order_receives;
          order_pays      = usd_for_sale;

          filled_short_or_limit = true;
          filled_call           = (usd_to_buy == usd_for_sale);
       }
       else // fill call
       {
          call_receives  = usd_to_buy;
          order_receives = usd_to_buy * match_price;
          call_pays      = order_receives;
          order_pays     = usd_to_buy;

          filled_call    = true;
       }

       auto old_call_itr = call_itr;
       if( filled_call ) ++call_itr;
       fill_order( *old_call_itr, call_pays, call_receives );
       if( current_is_limit )
       {
          auto old_limit_itr = !filled_call ? limit_itr++ : limit_itr;
          fill_order( *old_limit_itr, order_pays, order_receives );
       }
       else
       {
          auto old_short_itr = !filled_call ? short_itr++ : short_itr;
          fill_order( *old_short_itr, order_pays, order_receives );
       }
    } // whlie call_itr != call_end

    return filled_short_or_limit;
} FC_CAPTURE_AND_RETHROW() }

void generic_evaluator::cancel_order( const limit_order_object& order, bool create_virtual_op  )
{
   auto refunded = order.amount_for_sale();

   db().modify( order.seller(db()).statistics(db()),[&]( account_statistics_object& obj ){
      if( refunded.asset_id == asset_id_type() )
         obj.total_core_in_orders -= refunded.amount;
   });
   adjust_balance(order.seller, refunded);

   if( create_virtual_op )
   {
      // TODO: create a virtual cancel operation
   }

   db().remove( order );
}

/**
    for each short order, fill it at settlement price and place funds received into a total
    calculate the USD->BTS price and convert all USD balances to BTS at that price and subtract BTS from total
       - any fees accumulated by the issuer in the bitasset are forfeit / not redeemed
       - cancel all open orders with bitasset in it
       - any bonds with the bitasset as collateral get converted to BTS as collateral
       - any bitassets that use this bitasset as collateral are immediately settled at their feed price
       - convert all balances in bitasset to BTS and subtract from total
       - any prediction markets with usd as the backing get converted to BTS as the backing
    any BTS left over due to rounding errors is paid to accumulated fees
*/
void generic_evaluator::settle_black_swan( const asset_object& mia, const price& settlement_price )
{ try {
   elog( "BLACK SWAN!" );
   db().debug_dump();

   edump( (mia.symbol)(settlement_price) );

   const asset_object& backing_asset = mia.short_backing_asset(db());
   asset collateral_gathered = backing_asset.amount(0);

   const asset_dynamic_data_object& mia_dyn = mia.dynamic_asset_data_id(db());
   auto original_mia_supply = mia_dyn.current_supply;

   const call_order_index& call_index = db().get_index_type<call_order_index>();
   const auto& call_price_index = call_index.indices().get<by_price>();

    auto call_itr = call_price_index.lower_bound( price::min( mia.short_backing_asset, mia.id ) );
    auto call_end = call_price_index.upper_bound( price::max( mia.short_backing_asset, mia.id ) );
    while( call_itr != call_end )
    {
       auto pays = call_itr->get_debt() * settlement_price;
       wdump( (call_itr->get_debt() ) );
       collateral_gathered += pays;
       const auto&  order = *call_itr;
       ++call_itr;
       FC_ASSERT( fill_order( order, pays, order.get_debt() ) );
    }

   const limit_order_index& limit_index = db().get_index_type<limit_order_index>();
   const auto& limit_price_index = limit_index.indices().get<by_price>();

    // cancel all orders selling the market issued asset
    auto limit_itr = limit_price_index.lower_bound( price::max( mia.id, mia.short_backing_asset ) );
    auto limit_end = limit_price_index.upper_bound( ~mia.current_feed.call_limit );
    //auto limit_end = limit_price_index.upper_bound( price::max( asset_id_type(mia.id.instance()+1),
    //                                                            mia.short_backing_asset ) );
    while( limit_itr != limit_end )
    {
       const auto& order = *limit_itr;
       ilog( "CANCEL LIMIT ORDER" );
        idump((order));
       ++limit_itr;
       cancel_order( order );
    }

    limit_itr = limit_price_index.begin();
    //auto limit_end = limit_price_index.upper_bound( ~mia.current_feed.call_limit );
    limit_end = limit_price_index.end();
    while( limit_itr != limit_end )
    {
       if( limit_itr->amount_for_sale().asset_id == mia.id )
       {
          const auto& order = *limit_itr;
          ilog( "CANCEL_AGAIN" );
          edump((order));
          ++limit_itr;
          cancel_order( order );
       }
    }

   limit_itr = limit_price_index.begin();
   //auto limit_end = limit_price_index.upper_bound( ~mia.current_feed.call_limit );
   limit_end = limit_price_index.end();
   while( limit_itr != limit_end )
   {
      if( limit_itr->amount_for_sale().asset_id == mia.id )
      {
         const auto& order = *limit_itr;
         edump((order));
         ++limit_itr;
         cancel_order( order );
      }
   }

    asset total_mia_settled = mia.amount(0);
    auto& index = db().get_index_type<account_balance_index>().indices().get<by_asset>();
    auto range = index.equal_range(mia.get_id());
    for( auto itr = range.first; itr != range.second; ++itr )
    {
       auto mia_balance = itr->get_balance();
       if( mia_balance.amount > 0 )
       {
          adjust_balance(itr->owner, -mia_balance);
          auto settled_amount = mia_balance * settlement_price;
          idump( (mia_balance)(settled_amount)(settlement_price) );
          adjust_balance(itr->owner, settled_amount);
          total_mia_settled += mia_balance;
          collateral_gathered -= settled_amount;
       }
    }

    // TODO: convert collateral held in bonds
    // TODO: convert payments held in escrow
    // TODO: convert usd held as prediction market collateral

    db().modify( mia_dyn, [&]( asset_dynamic_data_object& obj ){
       total_mia_settled.amount += obj.accumulated_fees;
       obj.accumulated_fees = 0;
    });

    wlog( "====================== AFTER SETTLE BLACK SWAN UNCLAIMED SETTLEMENT FUNDS ==============\n" );
    wdump((collateral_gathered)(total_mia_settled)(original_mia_supply)(mia_dyn.current_supply));
    db().modify( mia.short_backing_asset(db()).dynamic_asset_data_id(db()), [&]( asset_dynamic_data_object& obj ){
       idump((collateral_gathered));
                 obj.accumulated_fees += collateral_gathered.amount;
                 idump((obj.accumulated_fees));
                 });

    FC_ASSERT( total_mia_settled.amount == original_mia_supply, "", ("total_settled",total_mia_settled)("original",original_mia_supply) );
} FC_CAPTURE_AND_RETHROW( (mia)(settlement_price) ) }

asset generic_evaluator::calculate_market_fee( const asset_object& trade_asset, const asset& trade_amount )
{
   assert( trade_asset.id == trade_amount.asset_id );

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.market_fee_percent;
   a /= BTS_MAX_MARKET_FEE_PERCENT;
   asset percent_fee = trade_asset.amount(a.to_uint64());

   if( percent_fee.amount > trade_asset.max_market_fee )
      percent_fee.amount = trade_asset.max_market_fee;
   else if( percent_fee.amount < trade_asset.min_market_fee )
      percent_fee.amount = trade_asset.min_market_fee;

   return percent_fee;
}

asset generic_evaluator::pay_market_fees( const asset_object& recv_asset, const asset& receives )
{
   auto issuer_fees = calculate_market_fee( recv_asset, receives );
   assert(issuer_fees <= receives );

   //Don't dirty undo state if not actually collecting any fees
   if( issuer_fees.amount > 0 )
   {
      const auto& recv_dyn_data = recv_asset.dynamic_asset_data_id(db());
      db().modify( recv_dyn_data, [&]( asset_dynamic_data_object& obj ){
                   idump((issuer_fees));
         obj.accumulated_fees += issuer_fees.amount;
      });
   }

   return issuer_fees;
}

asset generic_evaluator::get_balance(const account_object* account_obj, const asset_object* asset_obj)
{
   auto& index = db().get_index_type<account_balance_index>().indices().get<by_balance>();
   auto itr = index.find(boost::make_tuple(account_obj->get_id(), asset_obj->get_id()));
   if( itr == index.end() )
      return asset_obj->amount(0);
   return itr->get_balance();
}

void generic_evaluator::adjust_balance(account_id_type account, asset delta)
{ try {
   if( delta.amount == 0 )
      return;

   auto& index = db().get_index_type<account_balance_index>().indices().get<by_balance>();
   auto itr = index.find(boost::make_tuple(account, delta.asset_id));
   if(itr == index.end())
   {
      FC_ASSERT(delta.amount > 0);
      db().create<account_balance_object>([account,&delta](account_balance_object& b) {
         b.owner = account;
         b.asset_type = delta.asset_id;
         b.balance = delta.amount;
      });
   } else {
      FC_ASSERT(delta.amount > 0 || itr->get_balance() >= -delta);
      db().modify(*itr, [delta](account_balance_object& b) {
         b.adjust_balance(delta);
      });
   }
} FC_CAPTURE_AND_RETHROW( (account)(delta) ) }

void generic_evaluator::pay_order( const account_object& receiver, const asset& receives, const asset& pays )
{
   const auto& balances = receiver.statistics(db());
   db().modify( balances, [&]( account_statistics_object& b ){
         if( pays.asset_id == asset_id_type() )
            b.total_core_in_orders -= pays.amount;
   });
   adjust_balance(receiver.get_id(), receives);
}

/**
 *  For Market Issued assets Managed by Delegates, any fees collected in the MIA need
 *  to be sold and converted into CORE by accepting the best offer on the table.
 */
bool generic_evaluator::convert_fees( const asset_object& mia )
{
   if( mia.issuer != account_id_type() ) return false;
   return false;
}

bool generic_evaluator::fill_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const account_object& seller = order.seller(db());
   const asset_object& recv_asset = receives.asset_id(db());

   auto issuer_fees = pay_market_fees( recv_asset, receives );
   pay_order( seller, receives - issuer_fees, pays );

   db().push_applied_operation( fill_order_operation{ order.id, order.seller, pays, receives, issuer_fees } );

   if( pays == order.amount_for_sale() )
   {
      db().remove( order );
      return true;
   }
   else
   {
      db().modify( order, [&]( limit_order_object& b ) {
                             b.for_sale -= pays.amount;
                          });
      /**
       *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
       *  have hit the limit where the seller is asking for nothing in return.  When this
       *  happens we must refund any balance back to the seller, it is too small to be
       *  sold at the sale price.
       */
      if( order.amount_to_receive().amount == 0 )
      {
         cancel_order(order);
         return true;
      }
      return false;
   }
}
bool generic_evaluator::fill_order( const call_order_object& order, const asset& pays, const asset& receives )
{ try {
   idump((pays)(receives)(order));
   assert( order.get_debt().asset_id == receives.asset_id );
   assert( order.get_collateral().asset_id == pays.asset_id );
   assert( order.get_collateral() >= pays );

   optional<asset> collateral_freed;
   db().modify( order, [&]( call_order_object& o ){
            o.debt       -= receives.amount;
            o.collateral -= pays.amount;
            if( o.debt == 0 )
            {
              collateral_freed = o.get_collateral();
              o.collateral = 0;
            }
       });
   const asset_object& mia = receives.asset_id(db());
   assert( mia.is_market_issued() );

   const asset_dynamic_data_object& mia_ddo = mia.dynamic_asset_data_id(db());

   db().modify( mia_ddo, [&]( asset_dynamic_data_object& ao ){
       idump((receives));
        ao.current_supply -= receives.amount;
      });

   const account_object& borrower = order.borrower(db());
   if( collateral_freed || pays.asset_id == asset_id_type() )
   {
      const account_statistics_object& borrower_statistics = borrower.statistics(db());
      if( collateral_freed )
         adjust_balance(borrower.get_id(), *collateral_freed);
      db().modify( borrower_statistics, [&]( account_statistics_object& b ){
              if( collateral_freed && collateral_freed->amount > 0 )
                b.total_core_in_orders -= collateral_freed->amount;
              else if( pays.asset_id == asset_id_type() )
                b.total_core_in_orders -= pays.amount;
              assert( b.total_core_in_orders >= 0 );
           });
   }

   if( collateral_freed )
   {
      db().remove( order );
   }

   return collateral_freed.valid();
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }


bool generic_evaluator::fill_order( const short_order_object& order, const asset& pays, const asset& receives )
{ try {
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const call_order_index& call_index = db().get_index_type<call_order_index>();

   const account_object& seller = order.seller(db());
   const asset_object& recv_asset = receives.asset_id(db());
   const asset_object& pays_asset = pays.asset_id(db());
   assert( pays_asset.is_market_issued() );

   auto issuer_fees = pay_market_fees( recv_asset, receives );

   bool filled               = pays == order.amount_for_sale();
   auto seller_to_collateral = filled ? order.get_collateral() : pays * order.sell_price;
   auto buyer_to_collateral  = receives - issuer_fees;

   if( receives.asset_id == asset_id_type() )
   {
      const auto& statistics = seller.statistics(db());
      db().modify( statistics, [&]( account_statistics_object& b ){
             b.total_core_in_orders += buyer_to_collateral.amount;
      });
   }

   db().modify( pays_asset.dynamic_asset_data_id(db()), [&]( asset_dynamic_data_object& obj ){
      obj.current_supply += pays.amount;
   });

   const auto& call_account_index = call_index.indices().get<by_account>();
   auto call_itr = call_account_index.find(  boost::make_tuple(order.seller, pays.asset_id) );
   if( call_itr == call_account_index.end() )
   {
      db().create<call_order_object>( [&]( call_order_object& c ){
         c.borrower    = seller.id;
         c.collateral  = seller_to_collateral.amount + buyer_to_collateral.amount;
         c.debt        = pays.amount;
         c.maintenance_collateral_ratio = order.maintenance_collateral_ratio;
         c.call_price  = price::max(seller_to_collateral.asset_id, pays.asset_id);
         c.update_call_price();
      });
   }
   else
   {
      db().modify( *call_itr, [&]( call_order_object& c ){
         c.debt       += pays.amount;
         c.collateral += seller_to_collateral.amount + buyer_to_collateral.amount;
         c.maintenance_collateral_ratio = order.maintenance_collateral_ratio;
         c.update_call_price();
      });
   }

   if( filled )
   {
      db().remove( order );
   }
   else
   {
      db().modify( order, [&]( short_order_object& b ) {
         b.for_sale -= pays.amount;
         b.available_collateral -= seller_to_collateral.amount;
         assert( b.available_collateral > 0 );
         assert( b.for_sale > 0 );
      });

      /**
       *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
       *  have hit the limit where the seller is asking for nothing in return.  When this
       *  happens we must refund any balance back to the seller, it is too small to be
       *  sold at the sale price.
       */
      if( order.amount_to_receive().amount == 0 )
      {
         adjust_balance(seller.get_id(), order.get_collateral());
         if( order.get_collateral().asset_id == asset_id_type() )
         {
            const auto& statistics = seller.statistics(db());
            db().modify( statistics, [&]( account_statistics_object& b ){
                 b.total_core_in_orders -= order.available_collateral;
            });
         }

         db().remove( order );
         filled = true;
      }
   }
   return filled;
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }

bool generic_evaluator::fill_order(const force_settlement_object& settle, const asset& pays, const asset& receives)
{ try {
   bool filled = false;

   if( pays < settle.balance )
   {
      db().modify(settle, [&pays](force_settlement_object& s) {
         s.balance -= pays;
      });
      filled = false;
   } else {
      db().remove(settle);
      filled = true;
   }
   adjust_balance(settle.owner, receives);

   return filled;
} FC_CAPTURE_AND_RETHROW( (settle)(pays)(receives) ) }

} }
