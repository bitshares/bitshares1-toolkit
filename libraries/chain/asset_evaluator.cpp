#include <bts/chain/asset_evaluator.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/database.hpp>

#include <functional>

namespace bts { namespace chain {
object_id_type asset_create_evaluator::do_evaluate( const asset_create_operation& op )
{ try {
   database& d = db();

   const auto& chain_parameters = d.get_global_properties().parameters;
   FC_ASSERT( op.feed_publishers.size() <= chain_parameters.maximum_asset_feed_publishers );
   FC_ASSERT( op.whitelist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );
   FC_ASSERT( op.blacklist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );

   // Check that all authorities do exist
   for( auto id : op.feed_publishers )
      d.get_object(id);
   for( auto id : op.whitelist_authorities )
      d.get_object(id);
   for( auto id : op.blacklist_authorities )
      d.get_object(id);

   auto& asset_indx = db().get_index_type<asset_index>();
   auto asset_symbol_itr = asset_indx.indices().get<by_symbol>().find( op.symbol );
   FC_ASSERT( asset_symbol_itr == asset_indx.indices().get<by_symbol>().end() );

   core_fee_paid -= op.calculate_fee(d.current_fee_schedule()).value/2;
   assert( core_fee_paid >= 0 );

   FC_ASSERT( d.find_object(op.short_backing_asset) != nullptr );

   FC_ASSERT( op.feed_lifetime_seconds > chain_parameters.block_interval );
   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type asset_create_evaluator::do_apply( const asset_create_operation& op )
{
   const asset_dynamic_data_object& dyn_asset =
      db().create<asset_dynamic_data_object>( [&]( asset_dynamic_data_object& a ) {
         a.current_supply = 0;
         a.fee_pool = op.calculate_fee(db().current_fee_schedule()).value / 2;
      });

   auto next_asset_id = db().get_index_type<asset_index>().get_next_id();

   const asset_object& new_asset =
     db().create<asset_object>( [&]( asset_object& a ) {
         a.symbol = op.symbol;
         a.max_supply = op.max_supply;
         a.market_fee_percent = op.market_fee_percent;
         a.max_market_fee = op.max_market_fee;
         a.min_market_fee = op.min_market_fee;
         a.flags = op.flags;
         a.issuer_permissions = op.permissions;
         a.short_backing_asset = op.short_backing_asset;
         a.issuer = op.issuer;
         a.core_exchange_rate = op.core_exchange_rate;
         a.core_exchange_rate.base.asset_id = 0;
         a.core_exchange_rate.quote.asset_id = next_asset_id;
         a.dynamic_asset_data_id = dyn_asset.id;
         a.force_settlement_delay_sec = op.force_settlement_delay_sec;
         a.force_settlement_offset_percent = op.force_settlement_offset_percent;
         a.feed_lifetime_sec = op.feed_lifetime_seconds;
         std::transform(op.feed_publishers.begin(), op.feed_publishers.end(),
                        std::inserter(a.feeds, a.feeds.end()),
                        [this](account_id_type id) {
                           return make_pair(id, make_pair(db().head_block_time(), price_feed()));
                        });
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

   to_account = &o.issue_to_account(d);

   if( a.flags & white_list )
   {
      FC_ASSERT( to_account->is_authorized_asset( a ) );
   }

   asset_dyn_data = &a.dynamic_asset_data_id(d);
   FC_ASSERT( (asset_dyn_data->current_supply + o.asset_to_issue.amount) <= a.max_supply );

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type asset_issue_evaluator::do_apply( const asset_issue_operation& o )
{
   db().adjust_balance( o.issue_to_account, o.asset_to_issue );

   db().modify( *asset_dyn_data, [&]( asset_dynamic_data_object& data ){
        data.current_supply += o.asset_to_issue.amount;
   });

   return object_id_type();
}

object_id_type asset_fund_fee_pool_evaluator::do_evaluate(const asset_fund_fee_pool_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_id(d);

   asset_dyn_data = &a.dynamic_asset_data_id(d);

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type asset_fund_fee_pool_evaluator::do_apply(const asset_fund_fee_pool_operation& o)
{
   db().adjust_balance(o.from_account, -o.amount);

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

   const auto& chain_parameters = d.get_global_properties().parameters;

   if( o.new_feed_publishers )
   {
      FC_ASSERT( a.is_market_issued(), "Cannot set feed publishers on a user-issued asset." );
      FC_ASSERT( o.new_feed_publishers->size() <= chain_parameters.maximum_asset_feed_publishers );
   }
   if( o.new_price_feed )
   {
      FC_ASSERT( a.is_market_issued() &&
                 (a.issuer != account_id_type() || (o.new_issuer && *o.new_issuer != account_id_type())) );
   }
   if( o.new_whitelist_authorities )
   {
      FC_ASSERT( o.new_whitelist_authorities->size() <= chain_parameters.maximum_asset_whitelist_authorities );
      for( auto id : *o.new_whitelist_authorities )
         d.get_object(id);
   }
   if( o.new_blacklist_authorities )
   {
      FC_ASSERT( o.new_blacklist_authorities->size() <= chain_parameters.maximum_asset_whitelist_authorities );
      for( auto id : *o.new_blacklist_authorities )
         d.get_object(id);
   }
   if( o.new_whitelist_authorities || o.new_blacklist_authorities )
   {
      FC_ASSERT( a.enforce_white_list() || (o.flags && (*o.flags & white_list)) );
   }

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

   FC_ASSERT( o.feed_lifetime_seconds > chain_parameters.block_interval );
   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

object_id_type asset_update_evaluator::do_apply(const asset_update_operation& o)
{
   db().modify(*asset_to_update, [&](asset_object& a) {
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
      if( o.new_feed_publishers )
      {
         //This is tricky because I have a set of publishers coming in, but a map of publisher to feed is stored.
         //I need to update the map such that the keys match the new publishers, but not munge the old price feeds from
         //publishers who are being kept.
         //First, remove any old publishers who are no longer publishers
         for( auto itr = a.feeds.begin(); itr != a.feeds.end(); ++itr )
            if( !o.new_feed_publishers->count(itr->first) )
               itr = a.feeds.erase(itr);
         //Now, add any new publishers
         for( auto itr = o.new_feed_publishers->begin(); itr != o.new_feed_publishers->end(); ++itr )
            if( !a.feeds.count(*itr) )
               a.feeds[*itr];
         a.update_median_feeds(db().head_block_time());
      }
      if( o.new_whitelist_authorities )
         a.whitelist_authorities = *o.new_whitelist_authorities;
      if( o.new_blacklist_authorities )
         a.blacklist_authorities = *o.new_blacklist_authorities;

      a.market_fee_percent = o.market_fee_percent;
      a.max_market_fee = o.max_market_fee;
      a.min_market_fee = o.min_market_fee;
      a.force_settlement_delay_sec = o.force_settlement_delay_sec;
      a.force_settlement_offset_percent = o.force_settlement_offset_percent;
      a.feed_lifetime_sec = o.feed_lifetime_seconds;
   });

   return object_id_type();
}

object_id_type asset_settle_evaluator::do_evaluate(const asset_settle_evaluator::operation_type& op)
{
   const database& d = db();
   asset_to_settle = &op.amount.asset_id(d);
   FC_ASSERT(asset_to_settle->is_market_issued());
   FC_ASSERT(d.get_balance(d.get(op.account), *asset_to_settle) >= op.amount);

   return d.get_index_type<force_settlement_index>().get_next_id();
}

object_id_type asset_settle_evaluator::do_apply(const asset_settle_evaluator::operation_type& op)
{
   database& d = db();
   d.adjust_balance(op.account, -op.amount);
   return d.create<force_settlement_object>([&](force_settlement_object& s) {
      s.owner = op.account;
      s.balance = op.amount;
      s.settlement_date = d.head_block_time() + asset_to_settle->force_settlement_delay_sec;
   }).id;
}

object_id_type asset_publish_feeds_evaluator::do_evaluate(const asset_publish_feed_operation& o)
{ try {
   database& d = db();

   const asset_object& quote = o.feed.call_limit.quote.asset_id(d);
   //Verify that this feed is for a market-issued asset and that asset is backed by the base
   FC_ASSERT(quote.is_market_issued() && quote.short_backing_asset == o.feed.call_limit.base.asset_id);
   //Verify that the publisher is authoritative to publish a feed
   if( quote.issuer == account_id_type() )
   {
      //It's a delegate-fed asset. Verify that publisher is an active delegate or witness.
      FC_ASSERT(d.get(account_id_type()).active.auths.count(o.publisher) ||
                d.get_global_properties().witness_accounts.count(o.publisher));
   } else {
      FC_ASSERT(quote.feeds.count(o.publisher));
   }

   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

object_id_type asset_publish_feeds_evaluator::do_apply(const asset_publish_feed_operation& o)
{ try {
   database& d = db();

   const asset_object& quote = d.get(o.feed.call_limit.quote.asset_id);
   // Store medians for this asset
   d.modify(quote, [&o,&d](asset_object& a) {
      a.feeds[o.publisher] = make_pair(d.head_block_time(), o.feed);
      a.update_median_feeds(d.head_block_time());
   });

   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

} } // bts::chain
