#pragma once
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

         share_type current_supply;
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

         /// Helper function to get an asset object with the given amount in this asset's type
         asset amount(share_type a)const { return asset(a, id); }

         /// Ticker symbol for this asset, i.e. "USD"
         string                  symbol;
         /// ID of the account which issued this asset.
         account_id_type         issuer;

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
         /// This is the currently active price feed. Only relevant for market-issued assets.
         price_feed current_feed;

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
   };

   struct by_symbol{};
   typedef multi_index_container<
      asset_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_unique< tag<by_symbol>, member<asset_object, string, &asset_object::symbol> >
      >
   > asset_object_multi_index_type;

   typedef generic_index<asset_object, asset_object_multi_index_type> asset_index;

} } // bts::chain
FC_REFLECT_DERIVED( bts::chain::asset_dynamic_data_object, (bts::db::object),
                    (current_supply)(burned)(accumulated_fees)(fee_pool) )

FC_REFLECT_DERIVED( bts::chain::asset_object,
                    (bts::db::annotated_object<bts::chain::asset_object>),
                    (symbol)
                    (issuer)
                    (max_supply)
                    (max_market_fee)
                    (min_market_fee)
                    (market_fee_percent)
                    (issuer_permissions)
                    (flags)
                    (short_backing_asset)
                    (core_exchange_rate)
                    (current_feed)
                    (dynamic_asset_data_id)
                  )
