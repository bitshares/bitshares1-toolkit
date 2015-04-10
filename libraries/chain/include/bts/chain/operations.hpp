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

   struct key_create_operation
   {
      account_id_type  fee_paying_account;
      asset            fee;
      static_variant<address,public_key_type> key_data;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      share_type calculate_fee( const fee_schedule_type& k )const{ return k.at( key_create_fee_type ); }
      void       validate()const;
   };

   struct account_create_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
      string          name;
      authority       owner;
      authority       active;
      key_id_type     voting_key;
      key_id_type     memo_key;

      /// Delegate IDs must be in sorted order
      vector<delegate_id_type> vote;


      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct account_update_operation
   {
      account_id_type                     account;
      asset                               fee;
      optional<authority>                 owner;
      optional<authority>                 active;
      optional<key_id_type>               voting_key;
      optional<key_id_type>               memo_key;

      /**
       * Delegate IDs must be in sorted order
       */
      optional<vector<delegate_id_type>>  vote;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   /**
    *  Used to move delegate pay from accumulated_income to their account balance.
    */
   struct delegate_withdraw_pay_operation
   {
      asset            fee;
      delegate_id_type from_delegate;
      account_id_type  to_account; ///< must be from_delegate->delegate_account
      share_type       amount;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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

      void       get_required_auth( flat_set<account_id_type>&, flat_set<account_id_type>& )const {}
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct transfer_operation
   {
      account_id_type from;
      account_id_type to;
      asset           amount;
      asset           fee; ///< same asset_id as amount.asset_id
      vector<char>    memo;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_create_operation
   {
      account_id_type         issuer; // same as fee paying account
      asset                   fee;
      string                  symbol;
      share_type              max_supply;
      uint8_t                 precision = 0; ///< number of digits to the right of decimal
      uint16_t                market_fee_percent = 0;
      uint16_t                permissions = 0;
      uint16_t                flags = 0;
      price                   core_exchange_rate; // used for the fee pool
      asset_id_type           short_backing_asset; // for bitassets, specifies what may be used as collateral.

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_fund_fee_pool_operation
   {
      account_id_type from_account;
      asset_id_type   asset_id;
      share_type      amount; ///< core asset
      asset           fee; ///< core asset

      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& account_set )const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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
      // If price limits are null, shorts and margin calls are disabled.
      optional<price_feed>       new_price_feed;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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
      time_point_sec  expiration;

      /** if this flag is set the entire order must be filled or
       * the operation is rejected.
       */
      bool            fill_or_kill = false;

      void            get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void            validate()const;
      share_type      calculate_fee( const fee_schedule_type& k )const;
      price           get_price()const { return amount_to_sell / min_to_receive; }
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
      account_id_type     fee_paying_account;
      asset               fee;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;

      /** convention: amount_to_sell / amount_to_receive */
      price      sell_price()const { return ~price::call_price(amount_to_sell, collateral, initial_collateral_ratio); }

      /** convention: amount_to_sell / amount_to_receive means we are
       * selling collateral to receive debt
       **/
      price call_price() const { return price::call_price(amount_to_sell, collateral, maintenance_collateral_ratio); }
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

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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
    */
   struct call_order_update_operation
   {
      account_id_type     funding_account; ///< pays fee, collateral, and cover
      asset               fee; ///< paid by funding_account
      asset               collateral_to_add; ///< the amount of collateral to add to the margin position
      asset               amount_to_cover; ///< the amount of the debt to be paid off
      uint16_t            maintenance_collateral_ratio = 0; ///< 0 means don't change, 1000 means feed

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   /**
    * @brief This operation is used to authorize accounts to hold and transact in a whitelisted asset.
    *
    * Whitelisted assets can only be held and transfered by explicitly authorized accounts. This operation is how that
    * authorization is granted or revoked. An asset issuer may publish this operation in order to authorize an account
    * to hold his asset by setting authorize_account to true. The issuer may also use this operation to revoke the
    * account's authorization by setting authorize_account to false.
    *
    * If authorize_account is set to true and the account is already authorized, or authorize_account is set to false
    * and the account is already not authorized, this operation will fail. In other words, this operation must change
    * the whitelist_account's authorization status in order to succeed.
    *
    * This operation must be signed by asset_id's issuer. authorize_account's signature is not required.
    */
   struct asset_whitelist_operation
   {
      account_id_type  issuer; ///< Must be asset_id->issuer
      asset_id_type    asset_id; ///< ID of the whitelist asset in question
      asset            fee;
      account_id_type  whitelist_account; ///< ID of the account to allow or disallow to hold the asset
      bool             authorize_account; ///< True if whitelist_account may hold and transact the asset; false otherwise

      void       get_required_auth( flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& )const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_issue_operation
   {
      account_id_type  issuer; ///< Must be asset_to_issue->asset_id->issuer
      asset            asset_to_issue;
      asset            fee;
      account_id_type  issue_to_account;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct delegate_create_operation
   {
      account_id_type                       delegate_account; // same as fee_paying account
      asset                                 fee;
      uint8_t                               pay_rate;  // 0 to 100%
      secret_hash_type                      first_secret_hash;
      key_id_type                           signing_key;
      uint8_t                               block_interval_sec = BTS_DEFAULT_BLOCK_INTERVAL;
      uint32_t                              max_block_size = BTS_DEFAULT_MAX_BLOCK_SIZE;
      uint32_t                              max_transaction_size = BTS_DEFAULT_MAX_TRANSACTION_SIZE;
      uint32_t                              max_sec_until_expiration = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION;
      fee_schedule_type                     fee_schedule;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct delegate_update_operation
   {
      account_id_type                                 delegate_account; ///< must match delegate_id->delegate_account
      delegate_id_type                                delegate_id;
      asset                                           fee; ///< paid by delegate_id->delegate_account
      optional<fee_schedule_type>                     fee_schedule;
      optional<relative_key_id_type>                  signing_key;
      uint8_t                                         pay_rate = 255; ///< 255 for unchanged
      uint8_t                                         block_interval_sec = BTS_DEFAULT_BLOCK_INTERVAL;
      uint32_t                                        maintenance_interval_sec = BTS_DEFAULT_MAINTENANCE_INTERVAL;
      uint32_t                                        max_transaction_size = BTS_DEFAULT_MAX_TRANSACTION_SIZE;
      uint32_t                                        max_block_size = BTS_DEFAULT_MAX_BLOCK_SIZE;
      uint16_t                                        max_undo_history_size = BTS_DEFAULT_MAX_UNDO_HISTORY;
      uint32_t                                        max_sec_until_expiration = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
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
     *
     * TODO: Support owner authorities, not just active
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

      void       get_required_auth( flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& )const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
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

      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
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
      bool              using_owner_authority;
      asset             fee;
      proposal_id_type  proposal;

      void       get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& owner_auth_set)const;
      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
   };
   ///@}

   /**
    *  This is a virtual operation that is created while matching orders and
    *  emited for the purpose of accurately tracking account history, acclerating
    *  reindex.
    */
   struct fill_order_operation
   {
      object_id_type      order_id;
      account_id_type     account_id;
      asset               pays;
      asset               receives;
      asset               fee; // paid by receiving account

      void            get_required_auth(flat_set<account_id_type>& active_auth_set , flat_set<account_id_type>&)const{}
      void            validate()const { FC_ASSERT( !"virtual operation" ); }
      share_type      calculate_fee( const fee_schedule_type& k )const { return share_type(); }
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
            asset_create_operation,
            asset_update_operation,
            asset_whitelist_operation,
            asset_issue_operation,
            asset_fund_fee_pool_operation,
            delegate_publish_feeds_operation,
            delegate_create_operation,
            delegate_update_operation,
            delegate_withdraw_pay_operation,
            proposal_create_operation,
            proposal_update_operation,
            proposal_delete_operation,
            fill_order_operation
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
            (fee_paying_account)(fee)
            (name)
            (owner)(active)(voting_key)(memo_key)
          )

FC_REFLECT( bts::chain::account_update_operation,
            (account)(fee)(owner)(active)(voting_key)(memo_key)(vote)
          )

FC_REFLECT( bts::chain::delegate_update_operation,
            (delegate_account)(delegate_id)(fee)(fee_schedule)(signing_key)(pay_rate)
            (block_interval_sec)(maintenance_interval_sec)(max_transaction_size)
            (max_block_size)(max_undo_history_size)(max_sec_until_expiration)
          )

FC_REFLECT( bts::chain::delegate_publish_feeds_operation,
            (delegate)(fee)(feeds) )

FC_REFLECT( bts::chain::limit_order_create_operation,
            (seller)(amount_to_sell)(fee)(min_to_receive)(expiration)(fill_or_kill)
          )
FC_REFLECT( bts::chain::fill_order_operation, (order_id)(account_id)(pays)(receives)(fee) )
FC_REFLECT( bts::chain::limit_order_cancel_operation,(fee_paying_account)(fee)(order) )
FC_REFLECT( bts::chain::short_order_cancel_operation,(fee_paying_account)(fee)(order) )
FC_REFLECT( bts::chain::short_order_create_operation, (seller)(fee)(amount_to_sell)(collateral)(initial_collateral_ratio)(maintenance_collateral_ratio) )
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
            (permissions)
            (flags)
            (core_exchange_rate)
            (short_backing_asset)
          )

FC_REFLECT( bts::chain::asset_update_operation,
            (issuer)(asset_to_update)(fee)(flags)(permissions)(new_issuer)(core_exchange_rate)(new_price_feed)
          )

FC_REFLECT( bts::chain::asset_whitelist_operation,
            (issuer)(asset_id)(fee)(whitelist_account)(authorize_account)
          )
FC_REFLECT( bts::chain::asset_issue_operation,
            (issuer)(asset_to_issue)(fee)(issue_to_account) )
FC_REFLECT( bts::chain::delegate_create_operation,
            (delegate_account)(fee)(pay_rate)
            (first_secret_hash)(signing_key)
            (block_interval_sec)(max_block_size)
            (max_transaction_size)(max_sec_until_expiration)
            (fee_schedule)
          )

FC_REFLECT( bts::chain::proposal_create_operation, (fee_paying_account)(fee)(proposed_ops) )
FC_REFLECT( bts::chain::proposal_update_operation, (fee_paying_account)(fee)(proposal)
            (active_approvals_to_add)(active_approvals_to_remove)(owner_approvals_to_add)(owner_approvals_to_remove) )
FC_REFLECT( bts::chain::proposal_delete_operation, (fee_paying_account)(using_owner_authority)(fee)(proposal) )
FC_REFLECT( bts::chain::asset_fund_fee_pool_operation, (from_account)(asset_id)(amount)(fee) );

FC_REFLECT( bts::chain::delegate_withdraw_pay_operation, (fee)(from_delegate)(to_account)(amount) )
