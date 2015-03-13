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


int generic_evaluator::match( const limit_order_object& usd, const limit_order_object& core )
{
   //wdump( (usd)(core) );
   assert( core.sell_price  <= ~usd.sell_price );
   assert( ~usd.sell_price >= core.sell_price );
   assert( usd.id > core.id );
   assert( usd.sell_price.quote.asset_id == core.sell_price.base.asset_id );
   assert( usd.sell_price.base.asset_id  == core.sell_price.quote.asset_id );
   assert( usd.for_sale > 0 && core.for_sale > 0 );

   auto match_price  = core.sell_price;
   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();
//   wdump( (usd_for_sale)(core_for_sale) );

   auto max_usd_pays = core_for_sale * match_price; // USD * PRICE => BTS
//   wdump( (usd_for_sale)(core_for_sale)(max_usd_pays) );
   assert( max_usd_pays.asset_id != core_for_sale.asset_id );

   auto usd_trade_amount = max_usd_pays;
   if( usd_trade_amount > usd_for_sale ) usd_trade_amount = usd_for_sale; 

   auto usd_pays     = usd_trade_amount;
   auto usd_receives = usd_trade_amount * match_price;
   auto core_receives = usd_pays;
   auto core_pays     = usd_receives;
//   wdump( (usd_pays)(usd_receives) );
//   wdump( (core_pays)(core_receives) );

   core_pays = std::min(core_pays, core_for_sale);
   usd_receives = core_pays;

//   wdump( (usd_pays)(usd_receives) );
//   wdump( (core_pays)(core_receives) );

   // TODO: test a case where the order price is so wacky that trade_amount == 0 or pays/receives amount == 0

   int result = 0;
   result |= fill_limit_order( usd, usd_pays, usd_receives );
   result |= fill_limit_order( core, core_pays, core_receives ) << 1;
   return result;
}

asset generic_evaluator::calculate_market_fee( const asset_object& trade_asset, const asset& trade_amount )
{
   assert( trade_asset.id == trade_amount.asset_id );

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.market_fee_percent;
   a /= BTS_MAX_MARKET_FEE_PERCENT;
   return trade_asset.amount(a.to_uint64());
}

bool generic_evaluator::fill_limit_order( const limit_order_object& order, const asset& pays, const asset& receives )
{
   //wdump( (order)(pays)(receives) );
   assert( order.amount_for_sale().asset_id == pays.asset_id );
   assert( pays.asset_id != receives.asset_id );

   const account_object& seller = order.seller(db());
   //const asset_object& pays_asset = pays.asset_id(db());
   const asset_object& recv_asset = receives.asset_id(db());

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

   const auto& balances = seller.balances(db());
   db().modify( balances, [&]( account_balance_object& b ){
         if( pays.asset_id == asset_id_type() ) b.total_core_in_orders -= pays.amount;
         b.add_balance( receives - issuer_fees );
   });

   if( pays.asset_id == asset_id_type() ) adjust_votes( seller.delegate_votes, -pays.amount );

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
      return false;
   }
}

} }
