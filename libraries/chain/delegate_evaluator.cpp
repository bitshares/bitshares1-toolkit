#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type delegate_create_evaluator::do_evaluate( const delegate_create_operation& op )
{
   return object_id_type();
}

object_id_type delegate_create_evaluator::do_apply( const delegate_create_operation& op )
{
   const auto& vote_obj = db().create<vote_tally_object>( [&]( vote_tally_object& ){
         // initial vote is 0
   });
   const auto& feed_obj = db().create<delegate_feeds_object>( [&]( delegate_feeds_object& ) {} );

   const auto& new_del_object = db().create<delegate_object>( [&]( delegate_object& obj ){
         obj.delegate_account         = op.delegate_account;
         obj.vote                     = vote_obj.id;
         obj.feeds                    = feed_obj.id;
   });
   return new_del_object.id;
}

object_id_type delegate_publish_feeds_evaluator::do_evaluate(const delegate_publish_feeds_operation& o)
{ try {
   database& d = db();
   const delegate_object& del = o.delegate(d);

   for( const price_feed& feed : o.feeds )
   {
      const asset_object& quote = feed.call_limit.quote.asset_id(d);
      //Verify that this feed is for a market-issued asset and that asset is backed by the base
      FC_ASSERT(quote.is_market_issued() && quote.short_backing_asset == feed.call_limit.base.asset_id);
      //Verify that this is a delegate-fed asset
      FC_ASSERT(quote.issuer == account_id_type());
   }

   feed_box = &del.feeds(d);

   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

object_id_type delegate_publish_feeds_evaluator::do_apply(const delegate_publish_feeds_operation& o)
{ try {
   database& d = db();

   d.modify<delegate_feeds_object>( *feed_box, [this,o]( delegate_feeds_object& fobj) {
      for( const price_feed& feed : o.feeds )
         all_delegate_feeds[make_pair(feed.call_limit.base.asset_id,feed.call_limit.quote.asset_id)]
               .push_back(&fobj.set_feed(feed));
   });

   for( delegate_id_type delegate_id : d.get_global_properties().active_delegates )
      for( const price_feed& feed : o.feeds )
      {
         auto current_pair = make_pair(feed.call_limit.base.asset_id, feed.call_limit.quote.asset_id);
         if( delegate_id != o.delegate )
         {
            const price_feed* f = delegate_id(d).feeds(d).get_feed(current_pair.first, current_pair.second);
            if( f )
               all_delegate_feeds[current_pair].push_back(&*f);
         }
      }

   for( const price_feed& feed : o.feeds )
   {
      auto current_pair = make_pair(feed.call_limit.base.asset_id, feed.call_limit.quote.asset_id);
      vector<const price_feed*>& current_feeds = all_delegate_feeds[current_pair];
      price_feed& median_feed = median_feed_values[current_pair];

      if( current_feeds.empty() )
         continue;

      const auto median_itr = current_feeds.begin() + current_feeds.size() / 2;

      // *** Begin Median Calculations ***
      std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(),
                        [](const price_feed* a, const price_feed* b) {
         return a->call_limit < b->call_limit;
      });
      median_feed.call_limit = (*median_itr)->call_limit;

      std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(),
                        [](const price_feed* a, const price_feed* b) {
         return a->max_margin_period_sec < b->max_margin_period_sec;
      });
      median_feed.max_margin_period_sec = (*median_itr)->max_margin_period_sec;

      std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(),
                        [](const price_feed* a, const price_feed* b) {
         return a->required_initial_collateral < b->required_initial_collateral;
      });
      median_feed.required_initial_collateral = (*median_itr)->required_initial_collateral;

      std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(),
                        [](const price_feed* a, const price_feed* b) {
         return a->required_maintenance_collateral < b->required_maintenance_collateral;
      });
      median_feed.required_maintenance_collateral = (*median_itr)->required_maintenance_collateral;

      std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(),
                        [](const price_feed* a, const price_feed* b) {
         return a->short_limit < b->short_limit;
      });
      median_feed.short_limit = (*median_itr)->short_limit;
      // *** End Median Calculations ***

      // Store medians for this asset
      d.modify(current_pair.second(d), [&median_feed](asset_object& a) {
         a.current_feed = median_feed;
      });
   }

   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

} } // bts::chain
