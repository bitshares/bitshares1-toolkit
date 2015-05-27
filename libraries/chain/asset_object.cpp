#include <bts/chain/asset_object.hpp>

#include <fc/uint128.hpp>

#include <cmath>

using namespace bts::chain;

share_type asset_bitasset_data_object::max_force_settlement_volume(share_type current_supply) const
{
   if( options.maximum_force_settlement_volume == 0 )
      return 0;
   if( options.maximum_force_settlement_volume == BTS_100_PERCENT )
      return current_supply + force_settled_volume;

   fc::uint128 volume = current_supply.value + force_settled_volume.value;
   volume *= options.maximum_force_settlement_volume;
   volume /= BTS_100_PERCENT;
   return volume.to_uint64();
}

void bts::chain::asset_bitasset_data_object::update_median_feeds(time_point_sec current_time)
{
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

   if( current_feeds.empty() )
   {
      current_feed_publication_time = current_time;
      current_feed = price_feed();
      return;
   }
   if( current_feeds.size() == 1 )
   {
      current_feed = std::move(current_feeds.front());
      return;
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
   FC_ASSERT( (issuer_permissions & (disable_force_settle|global_settle)) ? issuer_permissions & market_issued : true, "", ("issuer_pemissions", issuer_permissions) );
   FC_ASSERT( (issuer_permissions & transfer_restricted) ? !(issuer_permissions & market_issued) : true, "market issued assets cannot be issuer restricted", ("issuer_pemissions", issuer_permissions) );
   // There must be no high bits in flags which are not also high in permissions.
   FC_ASSERT( !(flags & ~issuer_permissions ) );
   core_exchange_rate.validate();
   FC_ASSERT( core_exchange_rate.base.asset_id.instance.value == 0 ||
              core_exchange_rate.quote.asset_id.instance.value == 0 );

   if(!whitelist_authorities.empty() || !blacklist_authorities.empty())
      FC_ASSERT( flags & white_list );
   for( auto item : whitelist_markets )
   {
      FC_ASSERT( blacklist_markets.find(item) == blacklist_markets.end() );
   }
   for( auto item : blacklist_markets )
   {
      FC_ASSERT( whitelist_markets.find(item) == whitelist_markets.end() );
   }
}

void asset_object::bitasset_options::validate() const
{
   FC_ASSERT(force_settlement_offset_percent <= BTS_100_PERCENT);
   FC_ASSERT(maximum_force_settlement_volume <= BTS_100_PERCENT);
}


asset asset_object::amount_from_string(string amount_string) const
{ try {
   bool negative_found = false;
   bool decimal_found = false;
   for( const char c : amount_string )
   {
      if( isdigit( c ) )
         continue;

      if( c == '-' && !negative_found )
      {
         negative_found = true;
         continue;
      }

      if( c == '.' && !decimal_found )
      {
         decimal_found = true;
         continue;
      }

      FC_THROW( (amount_string) );
   }

   share_type satoshis = 0;

   share_type scaled_precision = 1;
   for( short i = 0; i < precision; ++i )
      scaled_precision *= 10;

   const auto decimal_pos = amount_string.find( '.' );
   const string lhs = amount_string.substr( negative_found, decimal_pos );
   if( !lhs.empty() )
      satoshis += fc::safe<int64_t>(std::stoll(lhs)) *= scaled_precision;

   if( decimal_found )
   {
      const size_t max_rhs_size = std::to_string( scaled_precision.value ).substr( 1 ).size();

      string rhs = amount_string.substr( decimal_pos + 1 );
      FC_ASSERT( rhs.size() <= max_rhs_size );

      while( rhs.size() < max_rhs_size )
         rhs += '0';

      if( !rhs.empty() )
         satoshis += std::stoll( rhs );
   }

   FC_ASSERT( satoshis <= BTS_BLOCKCHAIN_MAX_SHARES );

   if( negative_found )
      satoshis *= -1;

   return amount(satoshis);
   } FC_CAPTURE_AND_RETHROW( (amount_string) ) }

string asset_object::amount_to_string(share_type amount) const
{
   share_type scaled_precision = 1;
   for( short i = 0; i < precision; ++i )
      scaled_precision *= 10;
   assert(scaled_precision > 0);

   string result = fc::to_string(amount.value / scaled_precision.value);
   auto decimals = amount.value % scaled_precision.value;
   if( decimals )
      result += "." + fc::to_string(scaled_precision.value + decimals).erase(0,1);
   return result;
}
