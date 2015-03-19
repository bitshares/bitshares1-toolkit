#include<bts/chain/evaluator.hpp>
#include<bts/chain/transaction_evaluation_state.hpp>
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
      auto result = evaluate( op );
      if( apply ) result = this->apply( op );
      return result;
   }
   share_type generic_evaluator::pay_fee( account_id_type account_id, asset fee )
   { try {
      FC_ASSERT( fee.amount >= 0 );
      fee_paying_account = &account_id(db());
      FC_ASSERT( verify_authority( fee_paying_account, authority::active ) );

      fee_asset = &fee.asset_id(db());
      fee_asset_dyn_data = &fee_asset->dynamic_asset_data_id(db());
      FC_ASSERT( get_balance( fee_paying_account, fee_asset ) >= fee );

      asset fee_from_pool = fee;
      if( fee.asset_id != asset_id_type() )
      {
         fee_from_pool = fee * fee_asset->core_exchange_rate;
         FC_ASSERT( fee_from_pool.asset_id == asset_id_type() );
         FC_ASSERT( fee_from_pool.amount <= fee_asset_dyn_data->fee_pool - fees_paid[fee_asset].from_pool );
         fees_paid[fee_asset].from_pool += fee_from_pool.amount;
      }
      adjust_balance( fee_paying_account, fee_asset, -fee.amount );
      fees_paid[fee_asset].to_issuer += fee.amount;

      return fee_from_pool.amount;
   } FC_CAPTURE_AND_RETHROW( (account_id)(fee) ) }

   bool generic_evaluator::verify_authority( const account_object* a, authority::classification c )
   {
       return trx_state->check_authority( a, c );
   }

   bool generic_evaluator::verify_signature( const key_object* k )
   {
      FC_ASSERT( k != nullptr );
      return trx_state->_skip_signature_check || trx_state->signed_by.find( k->key_address() ) != trx_state->signed_by.end();
   }

   void generic_evaluator::adjust_balance( const account_object* for_account, const asset_object* for_asset, share_type delta )
   {
      delta_balance[for_account][for_asset] += delta;
   }
   /**
    *  Gets the balance of the account after all modifications that have been applied
    *  while evaluating this operation.
    */
   asset  generic_evaluator::get_balance( const account_object* for_account, const asset_object* for_asset )const
   {
      const auto& current_balance_obj = for_account->balances(db());
      auto current_balance = current_balance_obj.get_balance( for_asset->id );
      auto itr = delta_balance.find( for_account );
      if( itr == delta_balance.end() ) return current_balance;
      auto aitr = itr->second.find( for_asset );
      if( aitr == itr->second.end() ) return current_balance;
      return asset(current_balance.amount + aitr->second,for_asset->id);
   }

   void generic_evaluator::apply_delta_balances()
   {
      for( const auto& acnt : delta_balance )
      {
         const auto& balances = acnt.first->balances(db());
         db().modify(
             balances, [&]( account_balance_object& bal ){
                for( const auto& delta : acnt.second )
                {
                   if( delta.second > 0 )
                      bal.add_balance( asset(delta.second,delta.first->id) );
                   else if( delta.second < 0 )
                      bal.sub_balance( asset(-delta.second,delta.first->id) );
                }
         });
         auto itr = acnt.second.find( &db().get_core_asset() );
         if( itr != acnt.second.end() )
         {
            adjust_votes( acnt.first->delegate_votes, itr->second );
         }
      }
   }

   void generic_evaluator::adjust_votes( const vector<delegate_id_type>& delegate_ids, share_type delta )
   {
      database& d = db();
      for( auto id : delegate_ids )
      {
         d.modify( id(d).vote(d), [&]( delegate_vote_object& v ){
                   v.total_votes += delta;
            });
      }
   }

   void generic_evaluator::apply_delta_fee_pools()
   {
      for( const auto& fee : fees_paid )
      {
         const auto& dyn_asst_data = fee.first->dynamic_asset_data_id(db());
         db().modify( dyn_asst_data, [&]( asset_dynamic_data_object& dyn ){
                          dyn.fee_pool         -= fee.second.from_pool;
                          dyn.accumulated_fees += fee.second.to_issuer;
                     });
         if( dyn_asst_data.id != asset_id_type() )
            db().modify(dynamic_asset_data_id_type()(db()), [&]( asset_dynamic_data_object& dyn) {
               dyn.accumulated_fees += fee.second.from_pool;
            });
      }
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
   // wdump( (usd)(core)(match_price) );
   assert( usd.sell_price.quote.asset_id == core.sell_price.base.asset_id );
   assert( usd.sell_price.base.asset_id  == core.sell_price.quote.asset_id );
   assert( usd.for_sale > 0 && core.for_sale > 0 );

   //auto match_price  = core.sell_price;
   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();

   asset usd_pays, usd_receives, core_pays, core_receives;

   if( usd_for_sale <= core_for_sale * match_price )
   {
      core_receives = usd_for_sale;
      usd_receives  = usd_for_sale * match_price;
   }
   else if( core_for_sale < usd_for_sale * match_price )
   {
      usd_receives = core_for_sale;
      core_receives = core_for_sale * match_price;
   }
   else assert( !"unable to fill either order" );

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
    auto short_end = short_price_index.upper_bound( price::min( mia.id, mia.short_backing_asset ) );
    for( auto s = short_itr; s != short_end; ++s )
       wdump((*s));

    auto limit_itr = limit_price_index.lower_bound( price::max( mia.id, mia.short_backing_asset ) );
    auto limit_end = limit_price_index.upper_bound( price::min( mia.id, mia.short_backing_asset ) );
    for( auto l = limit_itr; l != limit_end; ++l )
       wdump((*l));


    auto call_itr = call_price_index.lower_bound( ~price::max( mia.id, mia.short_backing_asset ) );
    auto call_end = call_price_index.upper_bound( ~price::min( mia.id, mia.short_backing_asset ) );

    for( auto c = call_itr; c != call_end; ++c )
       wdump((*c));

    //auto call_end = call_price_index.upper_bound( ~price::max( mia.id, mia.short_backing_asset ) );
    if( call_itr == call_end )
    {
       elog( "      UNABLE TO FIND CALL ORDERS        " );
       return false;
    }
    bool filled_short_or_limit = false;


    while( call_itr != call_end )
    {
       wdump((*call_itr));
       bool  current_is_limit = true;
       bool  filled_call      = false;
       price match_price;
       asset usd_for_sale;
       if( limit_itr != limit_end )
       {
          assert( limit_itr != limit_price_index.end() );
          if( short_itr != short_end && limit_itr->sell_price < short_itr->sell_price )
          {
//             assert( short_itr != limit_price_index.end() );
             wdump((*short_itr));
             current_is_limit = false;
             match_price      = short_itr->sell_price;
             usd_for_sale     = short_itr->amount_for_sale();
          }
          else
          {
             wdump((*limit_itr));
             current_is_limit = true;
             match_price      = limit_itr->sell_price;
             usd_for_sale     = limit_itr->amount_for_sale();
          }
       }
       else if( short_itr != short_end )
       {
          wdump((*short_itr));
          assert( short_itr != short_price_index.end() );
          if( true  ) // check limit price
          {
             current_is_limit = false;
             match_price      = short_itr->sell_price;
             usd_for_sale     = short_itr->amount_for_sale();
          }
          else
          {
             elog("break");
             return filled_short_or_limit;
          }
       }
       else
       {
             elog("break");
          return filled_short_or_limit;
       }
       match_price.validate();
       idump((match_price));

       if( match_price > ~call_itr->call_price )
       {
          wdump((match_price)(~call_itr->call_price));
          edump((match_price.to_real())(">")((~call_itr->call_price).to_real()));
          return filled_short_or_limit;
          break;
       }
       else
       {
          elog( "CALL IT!" );
       }
       wdump( (match_price)(usd_for_sale) );

       auto usd_to_buy   = call_itr->get_debt();

       if( usd_to_buy * match_price > call_itr->get_collateral() )
       {
          elog( "black swan, we do not have enough collateral to cover at this price" );
          FC_ASSERT( !"BLACK SWAN" );
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








asset generic_evaluator::calculate_market_fee( const asset_object& trade_asset, const asset& trade_amount )
{
   assert( trade_asset.id == trade_amount.asset_id );

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.market_fee_percent;
   a /= BTS_MAX_MARKET_FEE_PERCENT;
   return trade_asset.amount(a.to_uint64());
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
         obj.accumulated_fees += issuer_fees.amount;
      });
   }

   return issuer_fees;
}

void generic_evaluator::pay_order( const account_object& receiver, const asset& receives, const asset& pays )
{
   const auto& balances = receiver.balances(db());
   db().modify( balances, [&]( account_balance_object& b ){
         if( pays.asset_id == asset_id_type() )
            b.total_core_in_orders -= pays.amount;
         b.add_balance( receives );
   });

   if( receives.asset_id == asset_id_type() )
      adjust_votes( receiver.delegate_votes, receives.amount );

   if( pays.asset_id == asset_id_type() )
      adjust_votes( receiver.delegate_votes, -pays.amount );
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
   wdump( (order)(pays)(receives) );
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const account_object& seller = order.seller(db());
   const asset_object& pays_asset = pays.asset_id(db());
   const asset_object& recv_asset = receives.asset_id(db());

   auto issuer_fees = pay_market_fees( recv_asset, receives );
   pay_order( seller, receives - issuer_fees, pays );

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
         adjust_balance( &seller, &pays_asset, order.for_sale );
         if( pays.asset_id == asset_id_type() )
         {
            const auto& balances = seller.balances(db());
            db().modify( balances, [&]( account_balance_object& b ){
                 b.total_core_in_orders -= order.for_sale;
            });
         }

         db().remove( order );
         return true;
      }
      return false;
   }
}
bool generic_evaluator::fill_order( const call_order_object& order, const asset& pays, const asset& receives )
{
   edump( (order)(pays)(receives) );
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
   const asset_object& mia                  = receives.asset_id(db());
   assert( mia.is_market_issued() );

   const asset_dynamic_data_object& mia_ddo = mia.dynamic_asset_data_id(db());

   db().modify( mia_ddo, [&]( asset_dynamic_data_object& ao ){
        ao.current_supply -= receives.amount;
      });

   const account_object& borrower = order.borrower(db());
   if( collateral_freed || pays.asset_id == asset_id_type() )
   {
      const account_balance_object& borrower_balances = borrower.balances(db());
      db().modify( borrower_balances, [&]( account_balance_object& b ){
              if( collateral_freed )
              {
                b.add_balance( *collateral_freed );
                b.total_core_in_orders -= collateral_freed->amount;
              }
              else if( pays.asset_id == asset_id_type() )
                b.total_core_in_orders -= pays.amount;
              assert( b.total_core_in_orders >= 0 );
           });
   }

   if( pays.asset_id == asset_id_type() )
      adjust_votes( borrower.delegate_votes, -pays.amount );

   if( collateral_freed )
   {
      db().remove( order );
   }

   return collateral_freed.valid();
}


bool generic_evaluator::fill_order( const short_order_object& order, const asset& pays, const asset& receives )
{ try {
   //idump( (order)(pays)(receives) );
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
      const auto& balances = seller.balances(db());
      db().modify( balances, [&]( account_balance_object& b ){
             b.total_core_in_orders += buyer_to_collateral.amount; //receives.amount;
      });
      adjust_votes( seller.delegate_votes, buyer_to_collateral.amount );
   }

   db().modify( pays_asset.dynamic_asset_data_id(db()), [&]( asset_dynamic_data_object& obj ){
                  obj.current_supply += pays.amount;
                });

   const auto& call_account_index = call_index.indices().get<by_account>();
   auto call_itr = call_account_index.find(  std::make_pair(order.seller, pays.asset_id) );
   //auto call_id = debts.get_call_order(pays.asset_id);
   if( call_itr == call_account_index.end() )
   {
      wlog( "." );
      const auto& call_obj = db().create<call_order_object>( [&]( call_order_object& c ){
             c.borrower    = seller.id;
             c.collateral  = seller_to_collateral.amount + buyer_to_collateral.amount;
             c.debt        = pays.amount;

             c.call_price  = order.call_price;
             fc::uint128 tmp( c.collateral.value );
             tmp *= order.maintenance_collateral_ratio - 1000;
             tmp /= 1000;
             FC_ASSERT( tmp <= BTS_MAX_SHARE_SUPPLY );

             c.call_price = (asset( tmp.to_uint64(), c.get_collateral().asset_id)) / c.get_debt();
        });
   }
   else
   {
      wdump( (order.available_collateral)(call_itr->collateral)(receives.amount) );
      db().modify( *call_itr, [&]( call_order_object& c ){
            c.debt       += pays.amount;
            c.collateral += seller_to_collateral.amount + buyer_to_collateral.amount;

            fc::uint128 tmp( c.collateral.value );
            tmp *= order.maintenance_collateral_ratio - 1000;
            tmp /= 1000;
            FC_ASSERT( tmp <= BTS_MAX_SHARE_SUPPLY );

            c.call_price = (asset( tmp.to_uint64(), c.get_collateral().asset_id)) / c.get_debt();
      });
   }

   if( filled )
   {
      //wdump((order.available_collateral)(seller_to_collateral));
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
         adjust_balance( &seller, &recv_asset, order.available_collateral);
         if( order.get_collateral().asset_id == asset_id_type() )
         {
            const auto& balances = seller.balances(db());
            db().modify( balances, [&]( account_balance_object& b ){
                 b.total_core_in_orders -= order.available_collateral;
            });
         }
         //wdump((order.available_collateral)(seller_to_collateral));

         db().remove( order );
         filled = true;
      }
   }
   // const auto& call_order = (*call_id)(db());
   //wdump( (order.available_collateral)(call_order.collateral)(receives.amount) );
   return filled;
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }

} }
