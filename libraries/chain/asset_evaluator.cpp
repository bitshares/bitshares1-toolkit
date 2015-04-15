#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type asset_create_evaluator::do_evaluate( const asset_create_operation& op )
{ try {
   database& d = db();

   FC_ASSERT( op.whitelist_authorities.size() <= d.get_global_properties().maximum_asset_whitelist_authorities );
   FC_ASSERT( op.blacklist_authorities.size() <= d.get_global_properties().maximum_asset_whitelist_authorities );

   // Check that all authorities do exist
   for( auto id : op.whitelist_authorities )
      d.get_object(id);
   for( auto id : op.blacklist_authorities )
      d.get_object(id);

   auto& asset_indx = db().get_index_type<asset_index>();
   auto asset_symbol_itr = asset_indx.indices().get<by_symbol>().find( op.symbol );
   FC_ASSERT( asset_symbol_itr == asset_indx.indices().get<by_symbol>().end() );

   auto bts_fee_paid = pay_fee( op.issuer, op.fee );
   bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   const asset_object& core_asset = d.get_core_asset();
   fees_paid[&core_asset].to_issuer -= bts_fee_required.value/2;
   assert( fees_paid[&core_asset].to_issuer >= 0 );

   FC_ASSERT( d.find_object(op.short_backing_asset) != nullptr );

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type asset_create_evaluator::do_apply( const asset_create_operation& op )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   const asset_dynamic_data_object& dyn_asset =
      db().create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object& a ) {
         a.current_supply = 0;
         a.fee_pool = bts_fee_required.value / 2;
      });

   auto next_asset_id = db().get_index_type<asset_index>().get_next_id();

   const asset_object& new_asset =
     db().create<asset_object>( [&]( asset_object& a ) {
         a.symbol = op.symbol;
         a.max_supply = op.max_supply;
         a.market_fee_percent = op.market_fee_percent;
         a.flags = op.flags;
         a.issuer_permissions = op.permissions;
         a.short_backing_asset = op.short_backing_asset;
         a.issuer = op.issuer;
         a.core_exchange_rate = op.core_exchange_rate;
         a.core_exchange_rate.base.asset_id = 0;
         a.core_exchange_rate.quote.asset_id = next_asset_id;
         a.dynamic_asset_data_id = dyn_asset.id;
         a.whitelist_authorities = op.whitelist_authorities;
         a.blacklist_authorities = op.blacklist_authorities;
      });
   assert( new_asset.id == next_asset_id );

   return next_asset_id;
}

object_id_type asset_issue_evaluator::do_evaluate( const asset_issue_operation& o )
{ try {
   database& d   = db();

   const asset_object& a = o.asset_to_issue.asset_id(d);
   FC_ASSERT( o.issuer == a.issuer );
   FC_ASSERT( !(a.issuer_permissions & market_issued) );

   auto bts_fee_paid = pay_fee( a.issuer, o.fee );
   bts_fee_required = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   to_account = &o.issue_to_account(d);

   if( a.flags & white_list )
   {
      FC_ASSERT( to_account->is_authorized_asset( a ) );
   }

   asset_dyn_data = &a.dynamic_asset_data_id(d);
   FC_ASSERT( (asset_dyn_data->current_supply + o.asset_to_issue.amount) <= a.max_supply );

   adjust_balance( to_account, &a, o.asset_to_issue.amount );

   return object_id_type();

} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type asset_issue_evaluator::do_apply( const asset_issue_operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   db().modify( *asset_dyn_data, [&]( asset_dynamic_data_object& data ){
        data.current_supply += o.asset_to_issue.amount;
   });

   return object_id_type();
}

object_id_type asset_fund_fee_pool_evaluator::do_evaluate(const asset_fund_fee_pool_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_id(d);

   auto bts_fee_paid = pay_fee( a.issuer, o.fee );
   bts_fee_required = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   asset_dyn_data = &a.dynamic_asset_data_id(d);
   adjust_balance(&o.from_account(d), &d.get_core_asset(), -o.amount);

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type asset_fund_fee_pool_evaluator::do_apply(const asset_fund_fee_pool_operation& o)
{
   apply_delta_balances();
   apply_delta_fee_pools();

   db().modify( *asset_dyn_data, [&]( asset_dynamic_data_object& data ) {
      data.fee_pool += o.amount;
   });

   return object_id_type();
}

object_id_type asset_update_evaluator::do_evaluate(const asset_update_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_to_update(d);
   asset_to_update = &a;
   FC_ASSERT( o.issuer == a.issuer, "", ("o.issuer", o.issuer)("a.issuer", a.issuer) );

   if( o.new_whitelist_authorities )
   {
      FC_ASSERT( o.new_whitelist_authorities->size() <= d.get_global_properties().maximum_asset_whitelist_authorities );
      for( auto id : *o.new_whitelist_authorities )
         d.get_object(id);
   }
   if( o.new_blacklist_authorities )
   {
      FC_ASSERT( o.new_blacklist_authorities->size() <= d.get_global_properties().maximum_asset_whitelist_authorities );
      for( auto id : *o.new_blacklist_authorities )
         d.get_object(id);
   }
   if( o.new_whitelist_authorities || o.new_blacklist_authorities )
   {
      FC_ASSERT( a.enforce_white_list() || o.flags && *o.flags & white_list );
   }

   auto bts_fee_paid = pay_fee( a.issuer, o.fee );
   bts_fee_required = o.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   if( o.new_issuer )
   {
      auto new_issuer = (*o.new_issuer)(d);
      FC_ASSERT(new_issuer.id != a.issuer);
   }

   if( o.permissions )
   {
      FC_ASSERT( *o.permissions != a.issuer_permissions );
      if( !o.flags )
         FC_ASSERT( (a.flags & *o.permissions) == a.flags );
      if( a.is_market_issued() ) FC_ASSERT(*o.permissions == market_issued);
      //There must be no bits set in o.permissions which are unset in a.issuer_permissions.
      FC_ASSERT(!(~a.issuer_permissions & *o.permissions));
   }
   if( o.flags )
   {
      FC_ASSERT( *o.flags != a.flags, "", ("a", a.flags) );
      FC_ASSERT( (*o.flags & a.issuer_permissions) == *o.flags );
      //Cannot change an asset to/from market_issued
      if( a.is_market_issued() ) FC_ASSERT(*o.flags == market_issued);
      else                       FC_ASSERT(~*o.flags & market_issued);
   }
   if( o.core_exchange_rate )
   {
      FC_ASSERT(!a.is_market_issued());
      FC_ASSERT(*o.core_exchange_rate != a.core_exchange_rate, "", ("e", a.core_exchange_rate));
   }
   if( o.new_price_feed )
   {
      FC_ASSERT(a.is_market_issued());
      FC_ASSERT(o.new_price_feed->call_limit.base.asset_id == a.short_backing_asset);
   }
   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

object_id_type asset_update_evaluator::do_apply(const asset_update_operation& o)
{
   apply_delta_balances();
   apply_delta_fee_pools();

   db().modify(*asset_to_update, [&o](asset_object& a) {
      if( o.new_issuer )
         a.issuer = *o.new_issuer;
      if( o.permissions )
         a.issuer_permissions = *o.permissions;
      if( o.flags )
         a.flags = *o.flags;
      if( o.core_exchange_rate )
         a.core_exchange_rate = *o.core_exchange_rate;
      if( o.new_price_feed )
         a.current_feed = *o.new_price_feed;
      if( o.new_whitelist_authorities )
         a.whitelist_authorities = *o.new_whitelist_authorities;
      if( o.new_blacklist_authorities )
         a.blacklist_authorities = *o.new_blacklist_authorities;
   });

   return object_id_type();
}

} } // bts::chain
