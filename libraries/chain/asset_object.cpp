#include <bts/chain/asset_object.hpp>

using namespace bts::chain;

void bts::chain::asset_bitasset_data_object::update_median_feeds(time_point_sec current_time)
{
   if( feeds.size() == 1 )
   {
      current_feed_publication_time = feeds.begin()->second.first;
      current_feed = feeds.begin()->second.second;
      return;
   }

   current_feed_publication_time = current_time;
   vector<std::reference_wrapper<const price_feed>> current_feeds;
   for( const pair<account_id_type, pair<time_point_sec,price_feed>>& f : feeds )
   {
      if( (current_time - f.second.first).to_seconds() < options.feed_lifetime_sec &&
          f.second.first != time_point_sec() )
      {
         current_feeds.emplace_back(f.second.second);
         current_feed_publication_time = std::min(current_feed_publication_time, f.second.first);
      }
   }

   // *** Begin Median Calculations ***
   price_feed median_feed;
   const auto median_itr = current_feeds.begin() + current_feeds.size() / 2;
#define CALCULATE_MEDIAN_VALUE(r, data, field_name) \
   std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(), \
                     [](const price_feed& a, const price_feed& b) { \
      return a.field_name < b.field_name; \
   }); \
   median_feed.field_name = median_itr->get().field_name;

   BOOST_PP_SEQ_FOR_EACH( CALCULATE_MEDIAN_VALUE, ~, BTS_PRICE_FEED_FIELDS )
#undef CALCULATE_MEDIAN_VALUE
   // *** End Median Calculations ***

   current_feed = median_feed;
}

void asset_object::asset_options::validate()const
{
   FC_ASSERT( max_supply > 0 );
   FC_ASSERT( max_supply <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( market_fee_percent <= BTS_100_PERCENT );
   FC_ASSERT( max_market_fee >= 0 && max_market_fee <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( min_market_fee >= 0 && min_market_fee <= BTS_MAX_SHARE_SUPPLY );
   // There must be no high bits in permissions whose meaning is not known.
   FC_ASSERT( !(issuer_permissions & ~ASSET_ISSUER_PERMISSION_MASK) );
   // There must be no high bits in flags which are not also high in permissions.
   FC_ASSERT( !(flags & ~issuer_permissions ) );
   core_exchange_rate.validate();
   FC_ASSERT( core_exchange_rate.base.asset_id.instance.value == 0 ||
              core_exchange_rate.quote.asset_id.instance.value == 0 );

   if(!whitelist_authorities.empty() || !blacklist_authorities.empty())
      FC_ASSERT( flags & white_list );
}

void asset_object::bitasset_options::validate() const
{
   FC_ASSERT(force_settlement_offset_percent <= BTS_100_PERCENT);
   FC_ASSERT(maximum_force_settlement_volume <= BTS_100_PERCENT);
}
