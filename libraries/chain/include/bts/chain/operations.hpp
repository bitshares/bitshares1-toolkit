#pragma once

#include <bts/chain/types.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/authority.hpp>

#include <fc/static_variant.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {

   bool is_valid_symbol( const string& symbol );
   bool is_valid_name( const string& s );
   bool is_premium_name( const string& n );
   bool is_cheap_name( const string& n );

   typedef fc::static_variant<object_id_type,asset> operation_result;

   struct balance_accumulator
   {
      void  adjust( account_id_type account, const asset& delta )
      {
         balance[ std::make_pair(account, delta.asset_id) ] += delta.amount;
      }
      flat_map< pair<account_id_type, asset_id_type>, share_type > balance;
   };

   struct key_create_operation
   {
      account_id_type  fee_paying_account;
      asset            fee;
      static_variant<address,public_key_type> key_data;

      account_id_type fee_payer()const { return fee_paying_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      share_type      calculate_fee( const fee_schedule_type& k )const{ return k.at( key_create_fee_type ); }
      void            validate()const;

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   struct account_create_operation
   {
      account_id_type registrar;
      asset           fee;

      /**
       *  If fee_paying_account->is_prime then referrer can be
       *  any other account that is also prime.  Otherwise referrer must
       *  equal fee_paying_account->referrer.
       */
      account_id_type referrer;
      uint8_t         referrer_percent = 0;

      string          name;
      authority       owner;
      authority       active;
      object_id_type  voting_key = key_id_type();
      object_id_type  memo_key = key_id_type();

      flat_set<vote_tally_id_type> vote;

      account_id_type fee_payer()const { return referrer; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief This operation is used to whitelist and blacklist accounts, primarily for transacting in whitelisted assets
    *
    * Accounts can freely specify opinions about other accounts, in the form of either whitelisting or blacklisting
    * them. This information is used in chain validation only to determine whether an account is authorized to transact
    * in an asset type which enforces a whitelist, but third parties can use this information for other uses as well,
    * as long as it does not conflict with the use of whitelisted assets.
    *
    * An asset which enforces a whitelist specifies a list of accounts to maintain its whitelist, and a list of
    * accounts to maintain its blacklist. In order for a given account A to hold and transact in a whitelisted asset S,
    * A must be whitelisted by at least one of S's whitelist_authorities and blacklisted by none of S's
    * blacklist_authorities. If A receives a balance of S, and is later removed from the whitelist(s) which allowed it
    * to hold S, or added to any blacklist S specifies as authoritative, A's balance of S will be frozen until A's
    * authorization is reinstated.
    *
    * This operation requires authorizing_account's signature, but not account_to_list's. The fee is paid by
    * authorizing_account.
    */
   struct account_whitelist_operation
   {
      enum account_listing {
         no_listing = 0x0, ///< No opinion is specified about this account
         white_listed = 0x1, ///< This account is whitelisted, but not blacklisted
         black_listed = 0x2, ///< This account is blacklisted, but not whitelisted
         white_and_black_listed = white_listed | black_listed ///< This account is both whitelisted and blacklisted
      };

      /// The account which is specifying an opinion of another account
      account_id_type authorizing_account;
      /// The account being opined about
      account_id_type account_to_list;
      /// The new white and blacklist status of account_to_list, as determined by authorizing_account
      /// This is a bitfield using values defined in the account_listing enum
      uint8_t new_listing;
      /// Paid by authorizing_account
      asset           fee;

      account_id_type fee_payer()const { return authorizing_account; }
      void get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void validate()const { FC_ASSERT( fee.amount >= 0 ); FC_ASSERT(new_listing < 0x4); }
      share_type calculate_fee(const fee_schedule_type& k)const { return k.at(account_whitelist_fee_type); }

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   struct account_update_operation
   {
      account_id_type                         account;
      asset                                   fee;
      optional<authority>                     owner;
      optional<authority>                     active;
      optional<object_id_type>                voting_key = key_id_type();
      optional<object_id_type>                memo_key = key_id_type();
      optional<flat_set<vote_tally_id_type>>  vote;

      /**
       * If set to true, upgrades the account to a prime account by setting the account's referrer to itself. This may
       * only be set to true when the account being modified is not already a prime account.
       */
      bool                                    upgrade_to_prime = false;

      account_id_type fee_payer()const { return account; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Create a delegate object, as a bid to hold a delegate seat on the network.
    *
    * Accounts which wish to become delegates may use this operation to create a delegate object which stakeholders may
    * vote on to approve its position as a delegate.
    */
   struct delegate_create_operation
   {
      /// The account which owns the delegate. This account pays the fee for this operation.
      account_id_type                       delegate_account;
      asset                                 fee;

      account_id_type fee_payer()const { return delegate_account; }
      void get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    *  @brief transfers the cashback rewards to balance
    *
    *  This is defined as a separate operation because cashback rewards would end up creating many
    *  small 'micro-payments' and this helps keep the ledger clean.
    */
   struct account_claim_cashback_operation
   {
      account_id_type account;
      asset           fee;
      share_type      amount;

      account_id_type fee_payer()const { return account; }
      void        get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void        validate()const;
      share_type  calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( account, asset(amount) );
      }
   };

   /**
    *  @brief transfers the account to another account while clearing the white list
    *
    *  In theory an account can be transferred by simply updating the authorities, but that kind
    *  of transfer lacks semantic meaning and is more often done to rotate keys without transferring
    *  ownership.   This operation is used to indicate the legal transfer of title to this account and
    *  a break in the operation history.
    *
    *  The account_id's owner/active/voting/memo authority should be set to new_owner
    *
    *  This operation will clear the account's whitelist statuses, but not the blacklist statuses.
    */
   struct account_transfer_operation
   {
      account_id_type account_id;
      account_id_type new_owner;
      asset           fee;

      account_id_type fee_payer()const { return account_id; }
      void        get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void        validate()const;
      share_type  calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Publish delegate-specified price feeds for market-issued assets
    *
    * Delegates use this operation to publish their price feeds for market-issued assets which are maintained by the
    * delegates. Each price feed is used to tune the market for a particular market-issued asset. For each value in the
    * feeds, the median across all delegate feeds for that asset is calculated and the market for the asset is
    * configured with the median of each value.
    *
    * The feeds in the operation each contain two prices: a call price limit and a short price limit. For each feed,
    * the call price is structured as (collateral asset) / (debt asset) and the short price is structured as (asset for
    * sale) / (collateral asset). Note that the asset IDs are opposite to eachother, so if we're publishing a feed for
    * USD, the call limit price will be CORE/USD and the short limit price will be USD/CORE.
    */
   struct delegate_publish_feeds_operation
   {
      delegate_id_type       delegate;
      asset                  fee; ///< paid for by delegate->delegate_account
      flat_set<price_feed>   feeds; ///< must be sorted with no duplicates

      account_id_type fee_payer()const { return account_id_type(); }
      void       get_required_auth( flat_set<account_id_type>&, flat_set<account_id_type>& )const {}
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Create a witness object, as a bid to hold a witness position on the network.
    *
    * Accounts which wish to become witnesses may use this operation to create a witness object which stakeholders may
    * vote on to approve its position as a witness.
    */
   struct witness_create_operation
   {
      /// The account which owns the delegate. This account pays the fee for this operation.
      account_id_type   witness_account;
      asset             fee;
      key_id_type       block_signing_key;
      secret_hash_type  initial_secret;

      account_id_type fee_payer()const { return witness_account; }
      void get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    *  Used to move witness pay from accumulated_income to their account balance.
    */
   struct witness_withdraw_pay_operation
   {
      /// The account to pay. Must match from_witness->witness_account. This account pays the fee for this operation.
      account_id_type  to_account;
      witness_id_type  from_witness;
      share_type       amount;
      asset            fee;

      account_id_type fee_payer()const { return to_account; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      void get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( to_account, amount );
      }
   };

   /**
    * @brief Used by delegates to update the global parameters of the blockchain.
    *
    * This operation allows the delegates to update the global parameters on the blockchain. These control various
    * tunable aspects of the chain, including block and maintenance intervals, maximum data sizes, the fees charged by
    * the network, etc.
    *
    * This operation may only be used in a proposed transaction, and a proposed transaction which contains this
    * operation must have a review period specified in the current global parameters before it may be accepted.
    */
   struct global_parameters_update_operation
   {
      chain_parameters new_parameters;
      asset fee;

      account_id_type fee_payer()const { return account_id_type(); }
      void get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
      { active_auth_set.insert(account_id_type()); }
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
      void get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   struct transfer_operation
   {
      account_id_type from;
      account_id_type to;
      asset           amount;
      asset           fee; ///< same asset_id as amount.asset_id
      vector<char>    memo;

      account_id_type fee_payer()const { return from; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
      void get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( from, -amount );
         acc.adjust( to, amount );
      }
   };

   struct asset_create_operation
   {
      /// This account must sign and pay the fee for this operation. Later, this account may update the asset
      account_id_type         issuer;
      asset                   fee;
      /// The ticker symbol of this asset
      string                  symbol;
      /// Maximum number of shares of this asset that may exist at any given point
      share_type              max_supply;
      /// Number of digits to the right of decimal point
      uint8_t                 precision = 0;
      /// Expressed in fixed-point notation, where 100 = 1%
      uint16_t                market_fee_percent = 0;
      /** If the percentage based fee is greater than max_market_fee then the max fee will be used */
      share_type              max_market_fee = BTS_MAX_SHARE_SUPPLY;
      /** If the percentage based fee is less than min_market_fee then the min fee will be used */
      share_type              min_market_fee;
      /// Bitfield specifying the flags the issuer is allowed to set
      uint16_t                permissions = 0;
      /// Bitfield specifying which flags are currently in effect
      uint16_t                flags = 0;
      /// Rate at which core asset from fee pool may be exhcanged for this asset
      price                   core_exchange_rate;
      /// The delay between the request and processing of a forced settlement
      uint32_t                force_settlement_delay_sec = BTS_DEFAULT_FORCE_SETTLEMENT_DELAY;
      /// Offset to apply to feed price when evaluating forced settlements
      uint16_t                force_settlement_offset_percent = BTS_DEFAULT_FORCE_SETTLEMENT_OFFSET;
      /// Only for market-issued assets; this speicifies which asset type is used to collateralize short sales
      asset_id_type           short_backing_asset;

      /// The set of accounts which manage the whitelist for this asset
      flat_set<account_id_type> whitelist_authorities;
      /// The set of accounts which manage the blacklist for this asset
      flat_set<account_id_type> blacklist_authorities;

      account_id_type fee_payer()const { return issuer; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Schedules a market-issued asset for automatic settlement
    *
    * Holders of market-issued assests may request a forced settlement for some amount of their asset. This means that
    * the specified sum will be locked by the chain and held for the settlement period, after which time the chain will
    * choose a margin posision holder and buy the settled asset using the margin's collateral. The price of this sale
    * will be based on the feed price for the market-issued asset being settled. The exact settlement price will be the
    * feed price at the time of settlement with an offset in favor of the margin position, where the offset is a
    * blockchain parameter set in the global_property_object.
    *
    * The fee is paid by account, and account must authorize this operation
    */
   struct asset_settle_operation
   {
      asset           fee;
      /// Account requesting the force settlement. This account pays the fee
      account_id_type account;
      /// Amount of asset to force settle. This must be a market-issued asset
      asset           amount;

      account_id_type fee_payer()const { return account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_fund_fee_pool_operation
   {
      account_id_type from_account;
      asset_id_type   asset_id;
      share_type      amount; ///< core asset
      asset           fee; ///< core asset

      account_id_type fee_payer()const { return from_account; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
      void       get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( fee_payer(), -amount );
      }
   };

   struct asset_update_operation
   {
      account_id_type issuer; ///< must be asset_to_update->issuer
      asset_id_type   asset_to_update;
      asset           fee;

      optional<uint16_t>         flags;
      optional<uint16_t>         permissions;
      optional<account_id_type>  new_issuer;
      optional<price>            core_exchange_rate;
      /// If price limits are null, shorts and margin calls are disabled.
      optional<price_feed>       new_price_feed;

      /// Expressed in fixed-point notation, where 100 = 1%
      uint16_t                market_fee_percent = 0;
      /** If the percentage based fee is greater than max_market_fee then the max fee will be used */
      share_type              max_market_fee = BTS_MAX_SHARE_SUPPLY;
      /** If the percentage based fee is less than min_market_fee then the min fee will be used */
      share_type              min_market_fee;
      /// The delay between the request and processing of a forced settlement
      uint32_t                force_settlement_delay_sec = BTS_DEFAULT_FORCE_SETTLEMENT_DELAY;
      /// Offset to apply to feed price when evaluating forced settlements
      uint16_t                force_settlement_offset_percent = BTS_DEFAULT_FORCE_SETTLEMENT_OFFSET;

      optional<flat_set<account_id_type>> new_whitelist_authorities;
      optional<flat_set<account_id_type>> new_blacklist_authorities;

      account_id_type fee_payer()const { return issuer; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   struct asset_issue_operation
   {
      account_id_type  issuer; ///< Must be asset_to_issue->asset_id->issuer
      asset            asset_to_issue;
      asset            fee;
      account_id_type  issue_to_account;

      account_id_type fee_payer()const { return issuer; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( issue_to_account, asset_to_issue );
      }
};

   /**
    *  @class limit_order_create_operation
    *  @brief instructs the blockchain to attempt to sell one asset for another
    *
    *  The blockchain will atempt to sell amount_to_sell.asset_id for as
    *  much min_to_receive.asset_id as possible.  The fee will be paid by
    *  the seller's account.  Market fees will apply as specified by the
    *  issuer of both the selling asset and the receiving asset as
    *  a percentage of the amount exchanged.
    *
    *  If either the selling asset or the receiving asset is white list
    *  restricted the order will only be created if the seller is on
    *  the white list of any asset type involved.
    *
    *  Market orders are matched in the order they are included
    *  in the block chaing.
    */
   struct limit_order_create_operation
   {
      account_id_type seller;
      asset           amount_to_sell;
      asset           fee;
      asset           min_to_receive;
      /**
       *  This order should expire if not filled by expiration
       */
      time_point_sec  expiration = time_point_sec::maximum();

      /** if this flag is set the entire order must be filled or
       * the operation is rejected.
       */
      bool            fill_or_kill = false;

      account_id_type fee_payer()const { return seller; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      price           get_price()const { return amount_to_sell / min_to_receive; }
      void            get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( seller, -amount_to_sell );
      }
   };


   /**
    *  Used to cancel an existing limit order, fee_pay_account and the
    *  account to receive the proceeds must be the same as order->seller
    *
    *  @return the amount actualy refunded
    */
   struct limit_order_cancel_operation
   {
      limit_order_id_type order;
      /** must be order->seller */
      account_id_type     fee_paying_account;
      asset               fee;

      account_id_type fee_payer()const { return fee_paying_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;

      void            get_balance_delta( balance_accumulator& acc, const operation_result& result )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( fee_payer(), result.get<asset>() );
      }
   };

   /**
    *  Define a new short order, if it is filled it will
    *  be merged with existing call orders for the same
    *  account.  If maintenance_collateral_ratio is set
    *  it will update any existing open call orders to
    *  use the new maintenance level.
    *
    *  When shorting you specify the total amount to sell
    *  and the amount of collateral along with the initial
    *  ratio.  The price it will sell at is (amount_to_sell/(collateral*initial_collateral_ratio/2000))
    */
   struct short_order_create_operation
   {
      /// The account placing a short order (this account must sign the transaction)
      account_id_type seller;
      /// The amount of market-issued asset to short sell
      asset           amount_to_sell;
      /// The fee paid by seller
      asset           fee;
      /// The amount of collateral to withdraw from the seller
      asset           collateral;
      /// Fixed point representation of initial collateral ratio, with three digits of precision
      /// Must be greater than or equal to the minimum specified by price feed
      uint16_t        initial_collateral_ratio    = BTS_DEFAULT_INITIAL_COLLATERAL_RATIO;
      /// Fixed point representation of maintenance collateral ratio, with three digits of precision
      /// Must be greater than or equal to the minimum specified by price feed
      uint16_t        maintenance_collateral_ratio = BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO;
      /// Expiration time for this order. Any unfilled portion of this order which is on the books at or past this time
      /// will automatically be canceled.
      time_point_sec  expiration = time_point_sec::maximum();

      account_id_type fee_payer()const { return seller; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      /** convention: amount_to_sell / amount_to_receive */
      price      sell_price()const { return ~price::call_price(amount_to_sell, collateral, initial_collateral_ratio); }

      /** convention: amount_to_sell / amount_to_receive means we are
       * selling collateral to receive debt
       **/
      price call_price() const { return price::call_price(amount_to_sell, collateral, maintenance_collateral_ratio); }

      void            get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( seller, -collateral );
      }
   };

   /**
    * Cancel the short order and return the balance to the
    * order->seller account.
    */
   struct short_order_cancel_operation
   {
      short_order_id_type order;
      account_id_type     fee_paying_account; ///< Must be order->seller
      asset               fee; ///< paid by order->seller

      account_id_type fee_payer()const { return fee_paying_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;

      void            get_balance_delta( balance_accumulator& acc, const operation_result& result )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( fee_payer(), result.get<asset>() );
      }
   };


   /**
    *  This operation can be used to add collateral, cover, and adjust the margin call price with a new maintenance
    *  collateral ratio.
    *
    *  The only way to "cancel" a call order is to pay off the balance due. The order is invalid if the payoff amount
    *  is greater than the amount due.
    *
    *  @note the call_order_id is implied by the funding_account and assets involved. This implies that the assets must
    *  have appropriate asset_ids, even if the amount is zero.
    *
    *  @note this operation can be used to force a market order using the collateral without requiring outside funds.
    */
   struct call_order_update_operation
   {
      account_id_type     funding_account; ///< pays fee, collateral, and cover
      asset               fee; ///< paid by funding_account
      asset               collateral_to_add; ///< the amount of collateral to add to the margin position
      asset               amount_to_cover; ///< the amount of the debt to be paid off
      uint16_t            maintenance_collateral_ratio = 0; ///< 0 means don't change, 1000 means feed

      account_id_type fee_payer()const { return funding_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( funding_account, -collateral_to_add );
         acc.adjust( funding_account, -amount_to_cover );
      }
   };

   /**
     * The Graphene Transaction Proposal Protocol
     *
     * Graphene allows users to propose a transaction which requires approval of multiple accounts in order to execute.
     * The user proposes a transaction using proposal_create_operation, then signatory accounts use
     * proposal_update_operations to add or remove their approvals from this operation. When a sufficient number of
     * approvals have been granted, the operations in the proposal are used to create a virtual transaction which is
     * subsequently evaluated. Even if the transaction fails, the proposal will be kept until the expiration time, at
     * which point, if sufficient approval is granted, the transaction will be evaluated a final time. This allows
     * transactions which will not execute successfully until a given time to still be executed through the proposal
     * mechanism. The first time the proposed transaction succeeds, the proposal will be regarded as resolved, and all
     * future updates will be invalid.
     *
     * The proposal system allows for arbitrarily complex or recursively nested authorities. If a recursive authority
     * (i.e. an authority which requires approval of 'nested' authorities on other accounts) is required for a
     * proposal, then a second proposal can be used to grant the nested authority's approval. That is, a second
     * proposal can be created which, when sufficiently approved, adds the approval of a nested authority to the first
     * proposal. This multiple-proposal scheme can be used to acquire approval for an arbitrarily deep authority tree.
     *
     * Note that at any time, a proposal can be approved in a single transaction if sufficient signatures are available
     * on the proposal_update_operation, as long as the authority tree to approve the proposal does not exceed the
     * maximum recursion depth. In practice, however, it is easier to use proposals to acquire all approvals, as this
     * leverages on-chain notification of all relevant parties that their approval is required. Off-chain
     * multi-signature approval requires some off-chain mechanism for acquiring several signatures on a single
     * transaction. This off-chain syncrhonization can be avoided using proposals.
     * @{
     */
   /**
    * op_wrapper is used to get around the circular definition of operation and proposals that contain them.
    */
   struct op_wrapper;
   /**
    * @brief The proposal_create_operation creates a transaction proposal, for use in multi-sig scenarios
    *
    * Creates a transaction proposal. The operations which compose the transaction are listed in order in proposed_ops,
    * and expiration_time specifies the time by which the proposal must be accepted or it will fail permanently. The
    * expiration_time cannot be farther in the future than the maximum expiration time set in the global properties
    * object.
    */
   struct proposal_create_operation
   {
       account_id_type    fee_paying_account;
       asset              fee;
       vector<op_wrapper> proposed_ops;
       time_point_sec     expiration_time;
       optional<uint32_t> review_period_seconds;

       /// Constructs a proposal_create_operation suitable for genesis proposals, with fee, expiration time and review
       /// period set appropriately.
       static proposal_create_operation genesis_proposal(const class database& db);

      account_id_type fee_payer()const { return fee_paying_account; }
      void       get_required_auth( flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& )const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }

      void            get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief The proposal_update_operation updates an existing transaction proposal
    *
    * This operation allows accounts to add or revoke approval of a proposed transaction. Signatures sufficient to
    * satisfy the authority of each account in approvals are required on the transaction containing this operation.
    *
    * If an account with a multi-signature authority is listed in approvals_to_add or approvals_to_remove, either all
    * required signatures to satisfy that account's authority must be provided in the transaction containing this
    * operation, or a secondary proposal must be created which contains this operation.
    *
    * NOTE: If the proposal requires only an account's active authority, the account must not update adding its owner
    * authority's approval. This is considered an error. An owner approval may only be added if the proposal requires
    * the owner's authority.
    *
    * If an account's owner and active authority are both required, only the owner authority may approve. An attempt to
    * add or remove active authority approval to such a proposal will fail.
    */
   struct proposal_update_operation
   {
      account_id_type            fee_paying_account;
      asset                      fee;
      proposal_id_type           proposal;
      flat_set<account_id_type>  active_approvals_to_add;
      flat_set<account_id_type>  active_approvals_to_remove;
      flat_set<account_id_type>  owner_approvals_to_add;
      flat_set<account_id_type>  owner_approvals_to_remove;
      flat_set<key_id_type>      key_approvals_to_add;
      flat_set<key_id_type>      key_approvals_to_remove;

      account_id_type fee_payer()const { return fee_paying_account; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
      void       get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief The proposal_delete_operation deletes an existing transaction proposal
    *
    * This operation allows the early veto of a proposed transaction. It may be used by any account which is a required
    * authority on the proposed transaction if that account's holder feels the proposal is ill-advised and he decides
    * he will never approve of it and wishes to put an end to all discussion of the issue. Because he is a required
    * authority, he could simply refuse to add his approval, but this would leave the topic open for debate until the
    * proposal expires. Using this operation, he can prevent any further breath from being wasted on such an absurd
    * proposal.
    */
   struct proposal_delete_operation
   {
      account_id_type   fee_paying_account;
      bool              using_owner_authority = false;
      asset             fee;
      proposal_id_type  proposal;

      account_id_type fee_payer()const { return fee_paying_account; }
      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
      void       get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };
   ///@}

   /**
    * This is a virtual operation that is created while matching orders and
    * emited for the purpose of accurately tracking account history, acclerating
    * reindex.
    */
   struct fill_order_operation
   {
      object_id_type      order_id;
      account_id_type     account_id;
      asset               pays;
      asset               receives;
      asset               fee; // paid by receiving account

      account_id_type fee_payer()const { return account_id; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const{}
      void            validate()const { FC_ASSERT( !"virtual operation" ); }
      share_type      calculate_fee( const fee_schedule_type& k )const { return share_type(); }
      void            get_balance_delta( balance_accumulator& acc )const {
         // acc.adjust( fee_payer(), -fee );  fee never actually entered the account, this is a virtual operation
         acc.adjust( account_id, receives );
      }
   };

   /**
    * @brief Create a new withdrawal permission
    *
    * This operation creates a withdrawal permission, which allows some authorized account to withdraw from an
    * authorizing account. This operation is primarily useful for scheduling recurring payments.
    *
    * Withdrawal permissions define withdrawal periods, which is a span of time during which the authorized account may
    * make a withdrawal. Only one withdrawal may occur per period for a given permission. Any subsequent withdrawals
    * will fail until the next period begins, even if the sum of all withdrawals within a given period does not exceed
    * the withdrawal limit.
    *
    * Withdrawal permissions authorize only a specific pairing, i.e. a permission only authorizes one specified
    * authorized account to withdraw from one specified authorizing account. Withdrawals are limited and may not exceet
    * the withdrawal limit. The withdrawal must be made in the same asset as the limit; attempts with withdraw any other
    * asset type will be rejected.
    *
    * The fee for this operation is paid by withdraw_from_account, and this account is required to authorize this
    * operation.
    */
   struct withdraw_permission_create_operation
   {
      asset             fee;
      /// The account authorizing withdrawals from its balances
      account_id_type   withdraw_from_account;
      /// The account authorized to make withdrawals from withdraw_from_account
      account_id_type   authorized_account;
      /// The maximum amount authorized_account is allowed to withdraw in a given withdrawal period
      asset             withdrawal_limit;
      /// Length of the withdrawal period in seconds
      uint32_t          withdrawal_period_sec;
      /// The number of withdrawal periods this permission is valid for
      uint32_t          periods_until_expiration;
      /// Time at which the first withdrawal period begins
      time_point_sec    period_start_time;

      account_id_type fee_payer()const { return withdraw_from_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Update an existing withdraw permission
    *
    * This oeration is used to update the settings for an existing withdrawal permission.
    *
    * Fee is paid by withdraw_from_account, which is required to authorize this operation
    */
   struct withdraw_permission_update_operation
   {
      asset                         fee;
      /// This account pays the fee. Must match permission_to_update->withdraw_from_account
      account_id_type               withdraw_from_account;
      /// The account authorized to make withdrawals. Must match permission_to_update->authorized_account
      account_id_type               authorized_account;
      /// ID of the permission which is being updated
      withdraw_permission_id_type   permission_to_update;
      /// New maximum amount the withdrawer is allowed to charge per withdrawal period
      asset                         withdrawal_limit;
      /// New length of the period between withdrawals
      uint32_t                      withdrawal_period_sec;
      /// New beginning of the next withdrawal period
      time_point_sec                starting_time;
      /// The new number of withdrawal periods for which this permission will be valid
      uint32_t                      periods_until_expiration;

      account_id_type fee_payer()const { return withdraw_from_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void         get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Withdraw from an account which has published a withdrawal permission
    *
    * This operation is used to withdraw from an account which has authorized such a withdrawal. It may be executed at
    * most once per withdrawal period for the given permission. On execution, amount_to_withdraw is transferred from
    * withdraw_from_account to withdraw_to_account, assuming amount_to_withdraw is within the withdrawal limit. The
    * withdrawal permission will be updated to note that the withdrawal for the current period has occurred, and
    * further withdrawals will not be permitted until the next withdrawal period, assuming the permission has not
    * expired. This operation may be executed at any time within the current withdrawal period.
    *
    * Fee is paid by withdraw_to_account, which is required to authorize this operation
    */
   struct withdraw_permission_claim_operation
   {
      /// Paid by withdraw_to_account
      asset                       fee;
      /// ID of the permission authorizing this withdrawal
      withdraw_permission_id_type withdraw_permission;
      /// Must match withdraw_permission->withdraw_from_account
      account_id_type             withdraw_from_account;
      /// Must match withdraw_permision->authorized_account
      account_id_type             withdraw_to_account;
      /// Amount to withdraw. Must not exceed withdraw_permission->withdrawal_limit
      asset                       amount_to_withdraw;
      /// Memo for withdraw_from_account. Should generally be encrypted with withdraw_from_account->memo_key
      vector<char>                memo;

      account_id_type fee_payer()const { return withdraw_to_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      void            get_balance_delta( balance_accumulator& acc )const { acc.adjust( fee_payer(), -fee ); }
   };

   /**
    * @brief Delete an existing withdrawal permission
    *
    * This operation cancels a withdrawal permission, thus preventing any future withdrawals using that permission.
    *
    * Fee is paid by withdraw_from_account, which is required to authorize this operation
    */
   struct withdraw_permission_delete_operation
   {
      asset                         fee;
      /// Must match withdrawal_permission->withdraw_from_account. This account pays the fee.
      account_id_type               withdraw_from_account;
      /// The account previously authorized to make withdrawals. Must match withdrawal_permission->authorized_account
      account_id_type               authorized_account;
      /// ID of the permission to be revoked.
      withdraw_permission_id_type   withdrawal_permission;

      account_id_type fee_payer()const { return withdraw_from_account; }
      void            get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;

      void            get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
      }
   };

   /**
    * Bond offers are objects that exist on the blockchain and can be
    * filled in full or in part by someone using the accept_bond_offer
    * operation. When the offer is accepted a new bond_object is
    * created that defines the terms of the loan.
    *
    *  @return bond_offer_id
    */
   struct create_bond_offer_operation
   {
      asset                   fee;
      account_id_type         creator;
      bool                    offer_to_borrow = false; // Offer to borrow if true, and offer to lend otherwise
      asset                   amount; // Amount to lend or secure depending on above
      price                   collateral_rate; // To derive amount of collateral or principle based on above
      /** after this time the lender can let the loan float or collect the collateral at will */
      uint32_t                min_loan_period_sec = 0; ///< the earliest the loan may be paid off
      uint32_t                loan_period_sec = 0;
      uint16_t                interest_apr = 0; ///< MAX_INTEREST_APR == 100% and is max value

      account_id_type   fee_payer()const { return creator; }
      void              get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void              validate()const;
      share_type        calculate_fee( const fee_schedule_type& k )const;
      void              get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( creator, -amount );
      }
   };

   /**
    *  Subtracts refund from bond_offer.amount and frees bond_offer if refund == bond_offer.amount
    */
   struct cancel_bond_offer_operation
   {
      asset                 fee;
      account_id_type       creator;
      bond_offer_id_type    offer_id;
      asset                 refund;

      account_id_type   fee_payer()const { return creator; }
      void              get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void              validate()const;
      share_type        calculate_fee( const fee_schedule_type& k )const;
      void              get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( creator, refund );
      }
   };

   /**
    *  @return new bond_id
    */
   struct accept_bond_offer_operation
   {
      asset               fee;
      account_id_type     claimer;
      bond_offer_id_type  offer_id;
      asset               amount; ///< the amount withdrawn from claimers account

      account_id_type   fee_payer()const { return claimer; }
      void              get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void              validate()const;
      share_type        calculate_fee( const fee_schedule_type& k )const;
      void              get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( claimer, -amount );
      }
   };

   /**
    *  After the loan period the lender can claim
    *  the collateral, prior to the loan period expiring
    *  the borrower can claim it by paying off the loan
    */
   struct claim_bond_collateral_operation
   {
      asset            fee;
      account_id_type  claimer;
      bond_id_type     bond_id;
      asset            payoff_amount;
      asset            collateral_claimed;

      account_id_type   fee_payer()const { return claimer; }
      void              get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const;
      void              validate()const;
      share_type        calculate_fee( const fee_schedule_type& k )const;
      void              get_balance_delta( balance_accumulator& acc )const
      {
         acc.adjust( fee_payer(), -fee );
         acc.adjust( claimer, -payoff_amount );
         acc.adjust( claimer, collateral_claimed );
      }
   };

   typedef fc::static_variant<
            transfer_operation,
            limit_order_create_operation,
            short_order_create_operation,
            limit_order_cancel_operation,
            short_order_cancel_operation,
            call_order_update_operation,
            key_create_operation,
            account_create_operation,
            account_update_operation,
            account_whitelist_operation,
            account_claim_cashback_operation,
            account_transfer_operation,
            asset_create_operation,
            asset_update_operation,
            asset_issue_operation,
            asset_fund_fee_pool_operation,
            asset_settle_operation,
            delegate_publish_feeds_operation,
            delegate_create_operation,
            witness_create_operation,
            witness_withdraw_pay_operation,
            proposal_create_operation,
            proposal_update_operation,
            proposal_delete_operation,
            withdraw_permission_update_operation,
            withdraw_permission_claim_operation,
            fill_order_operation,
            global_parameters_update_operation,
            create_bond_offer_operation
            /*
            * TODO: once methods on these ops are implemented
            cancel_bond_offer_operation,
            accept_bond_offer_operation,
            claim_bond_collateral_operation
            */
         > operation;

   /**
    *  Used to track the result of applying an operation and when it was applied.
    */
   struct applied_operation
   {
      operation        op;
      operation_result result;
      uint32_t         block_num;
      uint16_t         transaction_num;
      uint16_t         op_num;
   };

   /**
     * @brief Used to find accounts which must sign off on operations in a polymorphic manner
     */
   struct operation_get_required_auths
   {
      flat_set<account_id_type>& active_auth_set;
      flat_set<account_id_type>& owner_auth_set;
      operation_get_required_auths(flat_set<account_id_type>& active_auth_set,
                                   flat_set<account_id_type>& owner_auth_set)
         : active_auth_set(active_auth_set),
           owner_auth_set(owner_auth_set)
      {}
      typedef void result_type;
      template<typename T>
      void operator()(const T& v)const { v.get_required_auth(active_auth_set, owner_auth_set); }
   };

   /**
    * @brief Used to validate operations in a polymorphic manner
    */
   struct operation_validator
   {
      typedef void result_type;
      template<typename T>
      void operator()( const T& v )const { v.validate(); }
   };

   /**
    * @brief Used to calculate fees in a polymorphic manner
    *
    * If you wish to pay fees in an asset other than CORE, use the core_exchange_rate argument to specify the rate of
    * conversion you wish to use. The operation's fee will be calculated by multiplying the CORE fee by the provided
    * exchange rate. It is up to the caller to ensure that the core_exchange_rate converts to an asset accepted by the
    * delegates at a rate which they will accept.
    */
   struct operation_calculate_fee
   {
      const fee_schedule_type& fees;
      const price& core_exchange_rate;
      operation_calculate_fee( const fee_schedule_type& f, const price& core_exchange_rate = price::unit_price() )
         : fees(f),
           core_exchange_rate(core_exchange_rate)
      {}
      typedef share_type result_type;
      template<typename T>
      share_type operator()( const T& v )const { return (v.calculate_fee(fees) * core_exchange_rate).amount; }
   };

   /**
    * @brief Used to set fees in a polymorphic manner
    *
    * If you wish to pay fees in an asset other than CORE, use the core_exchange_rate argument to specify the rate of
    * conversion you wish to use. The operation's fee will be set by multiplying the CORE fee by the provided exchange
    * rate. It is up to the caller to ensure that the core_exchange_rate converts to an asset accepted by the delegates
    * at a rate which they will accept.
    */
   struct operation_set_fee
   {
      const fee_schedule_type& fees;
      const price& core_exchange_rate;
      operation_set_fee( const fee_schedule_type& f, const price& core_exchange_rate = price::unit_price() )
         : fees(f),
           core_exchange_rate(core_exchange_rate)
      {}
      typedef asset result_type;
      template<typename T>
      asset operator()( T& v )const { return v.fee = asset(v.calculate_fee(fees)) * core_exchange_rate; }
   };

   struct op_wrapper
   {
      public:
      op_wrapper(const operation& op = operation()):op(op){}
      operation op;

      void       validate()const { op.visit( operation_validator() ); }
      void       get_required_auth(flat_set<account_id_type>& active, flat_set<account_id_type>& owner) {
         op.visit(operation_get_required_auths(active, owner));
      }
      asset      set_fee( const fee_schedule_type& k ) { return op.visit( operation_set_fee( k ) ); }
      share_type calculate_fee( const fee_schedule_type& k )const { return op.visit( operation_calculate_fee( k ) ); }
   };

} } // bts::chain
FC_REFLECT( bts::chain::op_wrapper, (op) )

FC_REFLECT( bts::chain::key_create_operation,
            (fee_paying_account)(fee)
            (key_data)
          )

FC_REFLECT( bts::chain::account_create_operation,
            (registrar)(fee)
            (referrer)(referrer_percent)
            (name)
            (owner)(active)(voting_key)(memo_key)
          )

FC_REFLECT_TYPENAME( fc::flat_set<bts::chain::vote_tally_id_type> )
FC_REFLECT( bts::chain::account_update_operation,
            (account)(fee)(owner)(active)(voting_key)(memo_key)(vote)(upgrade_to_prime)
          )

FC_REFLECT_TYPENAME( bts::chain::account_whitelist_operation::account_listing)
FC_REFLECT_ENUM( bts::chain::account_whitelist_operation::account_listing,
                (no_listing)(white_listed)(black_listed)(white_and_black_listed))

FC_REFLECT( bts::chain::account_whitelist_operation, (authorizing_account)(account_to_list)(new_listing)(fee))
FC_REFLECT( bts::chain::account_claim_cashback_operation,      (account)(fee)(amount) )
FC_REFLECT( bts::chain::account_transfer_operation, (account_id)(new_owner)(fee) )

FC_REFLECT( bts::chain::delegate_create_operation,
            (delegate_account)(fee) )
FC_REFLECT( bts::chain::delegate_publish_feeds_operation,
            (delegate)(fee)(feeds) )

FC_REFLECT( bts::chain::witness_create_operation, (witness_account)(fee)(block_signing_key)(initial_secret) )
FC_REFLECT( bts::chain::witness_withdraw_pay_operation, (fee)(from_witness)(to_account)(amount) )

FC_REFLECT( bts::chain::limit_order_create_operation,
            (seller)(amount_to_sell)(fee)(min_to_receive)(expiration)(fill_or_kill)
          )
FC_REFLECT( bts::chain::fill_order_operation, (order_id)(account_id)(pays)(receives)(fee) )
FC_REFLECT( bts::chain::limit_order_cancel_operation,(fee_paying_account)(fee)(order) )
FC_REFLECT( bts::chain::short_order_cancel_operation,(fee_paying_account)(fee)(order) )
FC_REFLECT( bts::chain::short_order_create_operation, (seller)(fee)(amount_to_sell)(collateral)
            (initial_collateral_ratio)(maintenance_collateral_ratio)(expiration) )
FC_REFLECT( bts::chain::call_order_update_operation, (funding_account)(fee)(collateral_to_add)(amount_to_cover)(maintenance_collateral_ratio) )

FC_REFLECT( bts::chain::transfer_operation,
            (from)(to)(amount)(fee)(memo) )

FC_REFLECT( bts::chain::asset_create_operation,
            (issuer)
            (fee)
            (symbol)
            (max_supply)
            (precision)
            (market_fee_percent)
            (min_market_fee)
            (max_market_fee)
            (permissions)
            (flags)
            (core_exchange_rate)
            (short_backing_asset)
            (force_settlement_offset_percent)
          )
FC_REFLECT( bts::chain::asset_update_operation,
            (issuer)
            (asset_to_update)
            (fee)
            (flags)
            (permissions)
            (new_issuer)
            (core_exchange_rate)
            (new_price_feed)
            (market_fee_percent)
            (min_market_fee)
            (max_market_fee)
            (force_settlement_offset_percent)
//            (new_whitelist_authorities)
//            (new_blacklist_authorities)
          )
FC_REFLECT( bts::chain::asset_settle_operation, (fee)(account)(amount) )

FC_REFLECT( bts::chain::asset_issue_operation,
            (issuer)(asset_to_issue)(fee)(issue_to_account) )

FC_REFLECT( bts::chain::proposal_create_operation, (fee_paying_account)(fee)(expiration_time)
            (proposed_ops)(review_period_seconds) )
FC_REFLECT( bts::chain::proposal_update_operation, (fee_paying_account)(fee)(proposal)
            (active_approvals_to_add)(active_approvals_to_remove)(owner_approvals_to_add)(owner_approvals_to_remove)
            (key_approvals_to_add)(key_approvals_to_remove) )
FC_REFLECT( bts::chain::proposal_delete_operation, (fee_paying_account)(using_owner_authority)(fee)(proposal) )
FC_REFLECT( bts::chain::asset_fund_fee_pool_operation, (from_account)(asset_id)(amount)(fee) );

FC_REFLECT( bts::chain::global_parameters_update_operation, (new_parameters)(fee) );
FC_REFLECT( bts::chain::withdraw_permission_create_operation, (fee)(withdraw_from_account)(authorized_account)
            (withdrawal_limit)(withdrawal_period_sec)(periods_until_expiration)(period_start_time) )
FC_REFLECT( bts::chain::withdraw_permission_update_operation, (fee)(withdraw_from_account)(authorized_account)
            (permission_to_update)(withdrawal_limit)(withdrawal_period_sec)(starting_time)(periods_until_expiration) )
FC_REFLECT( bts::chain::withdraw_permission_claim_operation, (fee)(withdraw_permission)(withdraw_from_account)(withdraw_to_account)(amount_to_withdraw)(memo) );
FC_REFLECT( bts::chain::withdraw_permission_delete_operation, (fee)(withdraw_from_account)(authorized_account)
            (withdrawal_permission) )
FC_REFLECT( bts::chain::create_bond_offer_operation, (fee)(creator)(offer_to_borrow)(amount)(collateral_rate)(min_loan_period_sec)(loan_period_sec)(interest_apr) )
FC_REFLECT( bts::chain::cancel_bond_offer_operation, (fee)(creator)(offer_id)(refund) )
FC_REFLECT( bts::chain::accept_bond_offer_operation, (fee)(claimer)(offer_id)(amount) )
FC_REFLECT( bts::chain::claim_bond_collateral_operation, (fee)(claimer)(bond_id)(payoff_amount)(collateral_claimed) )


