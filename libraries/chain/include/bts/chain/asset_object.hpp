#pragma once
#include <boost/multi_index/composite_key.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/generic_index.hpp>


namespace bts { namespace chain {
   class account_object;
   using namespace bts::db;

   class asset_dynamic_data_object : public abstract_object<asset_dynamic_data_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_asset_dynamic_data_type;

         /// The number of shares currently in existence
         share_type current_supply;
         /// This tracks how much of the asset has been burned. burned + current_supply should always equal
         /// initial_supply
         share_type burned;
         share_type accumulated_fees; // fees accumulate to be paid out over time
         share_type fee_pool;         // in core asset
   };

   class asset_object : public bts::db::annotated_object<asset_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = asset_object_type;

         /// This function does not check if any registered asset has this symbol or not; it simply checks whether the
         /// symbol would be valid.
         /// @return true if symbol is a valid ticker symbol; false otherwise.
         static bool is_valid_symbol( const string& symbol );

         /// @return true if accounts must be on a whitelist in order to hold this asset; false otherwise.
         bool enforce_white_list()const { return flags & white_list; }
         /// @return true if this is a market-issued asset; false otherwise.
         bool is_market_issued()const { return flags & market_issued; }
         /// @return true if this asset charges a fee for the issuer on market operations; false otherwise
         bool charges_market_fees()const { return flags & charge_market_fee; }

         /// Helper function to get an asset object with the given amount in this asset's type
         asset amount(share_type a)const { return asset(a, id); }

         /// Ticker symbol for this asset, i.e. "USD"
         string                  symbol;
         /// ID of the account which issued this asset.
         account_id_type         issuer;

         /// @{ @group Market-Issued Asset Fields
         /// These fields are only relevant to market-issued assets.

         /// Feeds published for this asset. If issuer is not genesis, the keys in this map are the feed publishing
         /// accounts; otherwise, the feed publishers are the currently active delegates and witnesses and this map
         /// should be treated as an implementation detail. The timestamp on each feed is the time it was published.
         flat_map<account_id_type, pair<time_point_sec,price_feed>> feeds;
         /// Time before a price feed expires
         uint32_t feed_lifetime_sec;
         /// This is the currently active price feed, calculated as the median of values from the currently active
         /// feeds.
         price_feed current_feed;
         /// This is the publication time of the oldest feed which was factored into current_feed.
         fc::time_point_sec current_feed_publication_time;

         /// @}

         /// The maximum supply of this asset which may exist at any given time. This can be as large as
         /// BTS_MAX_SHARE_SUPPLY
         share_type              max_supply         = 0;
         /// When this asset is traded on the markets, this percentage of the total traded will be exacted and paid to
         /// the issuer. This is a fixed point value, representing hundredths of a percent, i.e. a value of 100 in this
         /// field means a 1% fee is charged on market trades of this asset.
         uint16_t                market_fee_percent = 0;
         share_type              max_market_fee;
         share_type              min_market_fee;

         /// The flags which the issuer has permission to update. See asset_issuer_permission_flags
         uint16_t                issuer_permissions;
         /// The currently active flags on this permission. See asset_issuer_permission_flags
         uint16_t                flags;

         /// The short backing asset is used as collateral when short selling this asset. Only relevant for market-
         /// issued assets.
         asset_id_type           short_backing_asset;

         /// When a user-issued asset is used to pay a fee, the blockchain must convert that asset to core asset in
         /// order to accept the fee. If this asset's fee pool is funded, the chain will automatically deposite fees in
         /// this asset to its accumulated fees, and withdraw from the fee pool the same amount as converted at the
         /// core exchange rate.
         price core_exchange_rate;
         /// This is the delay between the time a long requests settlement and the chain evaluates the settlement
         uint32_t force_settlement_delay_sec;
         /// This is the percent to adjust the feed price in the short's favor in the event of a forced settlement
         uint16_t force_settlement_offset_percent;

         /// Current supply, fee pool, and collected fees are stored in a separate object as they change frequently.
         dynamic_asset_data_id_type  dynamic_asset_data_id;

         /// A set of accounts which maintain whitelists to consult for this asset. If enforce_white_list() returns
         /// true, an account may only send, receive, trade, etc. in this asset if one of these accounts appears in its
         /// account_object::whitelisting_accounts field.
         flat_set<account_id_type> whitelist_authorities;
         /// A set of accounts which maintain blacklists to consult for this asset. If enforce_white_list() returns
         /// true, an account may only send, receive, trade, etc. in this asset if none of these accounts appears in
         /// its account_object::blacklisting_accounts field. If the account is blacklisted, it may not transact in
         /// this asset even if it is also whitelisted.
         flat_set<account_id_type> blacklist_authorities;

         asset_id_type get_id()const { return id; }

         time_point_sec feed_expiration_time()const { return current_feed_publication_time + feed_lifetime_sec; }
         bool feed_is_expired(time_point_sec current_time)const
         {
            return feed_expiration_time() >= current_time;
         }
         void update_median_feeds(time_point_sec current_time);
   };

   class force_settlement_object : public bts::db::annotated_object<force_settlement_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = force_settlement_object_type;

         account_id_type   owner;
         asset             balance;
         time_point_sec    settlement_date;
   };

   struct by_symbol;
   struct by_feed_expiration;
   typedef multi_index_container<
      asset_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_unique< tag<by_symbol>, member<asset_object, string, &asset_object::symbol> >,
         ordered_non_unique< tag<by_feed_expiration>,
            const_mem_fun< asset_object, time_point_sec, &asset_object::feed_expiration_time >
         >
      >
   > asset_object_multi_index_type;

   typedef generic_index<asset_object, asset_object_multi_index_type> asset_index;

   struct by_account;
   struct by_expiration;
   typedef multi_index_container<
      force_settlement_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_account>,
            member<force_settlement_object, account_id_type, &force_settlement_object::owner>
         >,
         ordered_non_unique< tag<by_expiration>,
            member<force_settlement_object, time_point_sec, &force_settlement_object::settlement_date>
         >
      >
   > force_settlement_object_multi_index_type;
   typedef generic_index<force_settlement_object, force_settlement_object_multi_index_type> force_settlement_index;

} } // bts::chain
FC_REFLECT_DERIVED( bts::chain::asset_dynamic_data_object, (bts::db::object),
                    (current_supply)(burned)(accumulated_fees)(fee_pool) )

FC_REFLECT_DERIVED( bts::chain::asset_object,
                    (bts::db::annotated_object<bts::chain::asset_object>),
                    (symbol)
                    (issuer)
                    (feeds)
                    (feed_lifetime_sec)
                    (current_feed)
                    (current_feed_publication_time)
                    (max_supply)
                    (market_fee_percent)
                    (max_market_fee)
                    (min_market_fee)
                    (issuer_permissions)
                    (flags)
                    (short_backing_asset)
                    (core_exchange_rate)
                    (force_settlement_delay_sec)
                    (force_settlement_offset_percent)
                    (dynamic_asset_data_id)
                    (whitelist_authorities)
                    (blacklist_authorities)
                  )

FC_REFLECT( bts::chain::force_settlement_object, (owner)(balance)(settlement_date) )
