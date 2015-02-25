#pragma once
#include <bts/chain/operations.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/asset.hpp>

namespace bts { namespace chain { 

   /**
    *   
    */
   enum asset_issuer_permission_flags
   {
      charge_transfer_fee = 0x01,
      charge_market_fee   = 0x02,
      white_list          = 0x04,
      halt_market         = 0x08,
      halt_transfer       = 0x10,
      override_authority  = 0x20,
      market_issued       = 0x40
   };

   /**
    *  There are many types of fees charged by the network
    *  for different operations. These fees are published by
    *  the delegates and can change over time.
    */
   enum fee_type
   {
      account_registration_fee_type, ///< the cost to register the cheapest non-free account
      asset_registration_fee_type, ///< the cost to register the cheapest asset
      market_fee_type, ///< a percentage charged on market orders
      transaction_fee_type, ///< a base price for every transaction
      data_fee_type, ///< a price per byte of user data
      delegate_registration_fee, ///< fixed fee for registering as a delegate, used to discourage frivioulous delegates
      signature_fee_type ///< a surcharge on transactions with more than 2 signatures.
   };

   /**
    *  @class create_account_operation
    *  @brief Creates a new account 
    *
    *  An account is a named pair authorities (permissions) that are assigned a name.   An
    *  authority is simply a N-of-M multi-signature specification. 
    *
    *  These authorities are separated into two categories: owner and active.  The *owner*
    *  authority is designed to be kept in "cold-storage" and is not required for day-to-day
    *  operations.   The *active* authority maintains spending control over the account. The
    *  *owner* may change the *active* authority at any time. 
    *
    *  Accounts are transferrable if signed by the owner and a new registration_fee is paid. 
    *  You "transfer" an account by "re-registering it".   
    *
    *  Names may only contain [a-z0-9] or '-' and must start with [a-z] and not have more
    *  than one '-' in a row.   A name can have up to 63 characters.  
    *
    *  Sub-accounts can be created by specifying a "/", ie: root/child/grandchild.   The root account active
    *  key has signing authority over all children.  The full name "root/child/grandchild" is part of
    *  the 63 character limit
    *
    *  The registration_fee depends upon the number of characters in the name and the
    *  current median registration_fee published by the delegates.  Names with more than
    *  8 characters or that include a number are free.  The registration fee can be
    *  calculated as follows registration_fee*10^(8-NUM_CHARACTERS). There is no registration 
    *  fee for child accounts.
    */
   struct create_account_operation
   {
       static const operation_type type;

       string          name;
       account_id_type paying_account;
       asset           registration_fee;

       vector< pair<public_key_type,uint16_t> > owner_register_keys; // new key/weight pair to add to owner
       vector< pair<public_key_type,uint16_t> > active_register_keys; // new key/weight pair to add to active
       vector< pair<public_key_type,uint16_t> > voter_register_keys; // new key/weight pair to add to voting
      
       authority                 owner;
       authority                 active;
       authority                 voting;
       public_key_type           memo; 

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true);
   };

   /**
    *  This operation will set the delegates that a particular account
    *  is voting for.  This is only valid if any of the voting, active, or
    *  owner authorities have signed.
    */
   struct delegate_vote_account 
   {
       static const operation_type type;

       account_id_type          account_id;
       vector<delegate_id_type> approved_delegates;
   };

   /**
    *  @class create_asset_operation
    *  @brief Creates a new asset 
    *
    *  An asset can be used for everything from stocks, to bonds, to
    *  rewards points, tickets or even a market-issued asset.
    *
    *  Every asset is uniquely identified by a symbol name that must
    *  be at least 3 characters [A-Z].  Symbol names are in high demand
    *  so reserving them carries a fee.  The fewer the characters the
    *  higher the fee.   Symbols longer than 8 characters are charged
    *  the minimum fee.  The forumla for calculating the fee is
    *  MIN_FEE*10^(8-NUM_CHARS).
    *
    *  Other properties such as *name* and *description* are for
    *  record keeping purposes only.  A name has a maximum length of
    *  63 characters and the description field is charged a data fee.
    *
    *  Ability to issue and update an asset depend entirely upon the
    *  issuer's owner and active keys.  
    *
    *  Assets can grant their issuers many different privileges so that
    *  the issuer can comply with all necessary regulatory compliance. Any
    *  of these permissions can be permanently revoked to protect the
    *  owners of the asset.   
    *
    *  BitAssets (aka Market Issued Assets) can be created by anyone provided
    *  they specify a set of feed producers. 
    *
    *  The issuer of an asset can specify a transfer fee that will be paid to
    *  the issuer on every transfer operation for the specified asset.
    *  
    *  The issuer can also specify a market fee as a percent of the proceeds
    *  of every order denominated this asset type.  This is designed to
    *  allow exchanges to make money in the traditional maner.
    *
    */
   struct create_asset_operation
   {
       static const operation_type type;

       string                  symbol;
       account_id_type         issuer;       /// the account that will pay the creation fee and has authority over this asset and receives all fees collected
       asset                   creation_fee; /// must match the expected creation fee
       asset                   data_fee;     /// must match the expected creation fee
       share_type              max_supply         = 0; // unlimited.
       uint8_t                 precision_digits   = 0; // 0 to 10
       uint16_t                market_fee_percent = 0; // 10,000 = 100%
       share_type              transfer_fee;       /// charged on all transfer involving the asset.
       uint32_t                issuer_permissions; /// freeze market, clawback @see asset_issuer_permission_flags
       uint32_t                flags; /// white_list, halt_market, halt_transfer @see asset_issuer_permission_flags
       /** max producers = 101 */
       vector<account_id_type> feed_producers; 
       asset_id_type           short_backing_asset; // for bitassets, specifies what may be used as collateral.

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  Used to update assets records.  
    *  
    *  This operation is only valid if signed by the owner key of the issuer. 
    *  Permissions can be removed but not added. 
    *  Flags can only be set if there exist proper permissions.
    *  Certain fee fields can only be specified if permissions permit. 
    *
    *  A data fee will be charged on the bytes in the description.
    */
   struct update_asset_operation
   {
      static const operation_type type;

      asset_id_type           asset_id;
      account_id_type         new_issuer; // 0 means don't change
      uint16_t                new_market_fee_percent = -1; // default value means don't change
      share_type              new_transfer_fee       = -1; // default value means don't change
      uint32_t                new_issuer_permissions = -1; // default value means don't change
      uint32_t                new_flags   = -1; // default value means don't change
      vector<account_id_type> feed_producers; 
      asset                   data_fee;     /// must match the expected data fee

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  @class update_asset_white_list_operation
    *  @briefs adds or removes an accounts ability to own an asset.
    *
    *  If an account is deauthorized its funds are frozen until
    *  reauthorized.
    */
   struct update_asset_white_list_operation
   {
      static const operation_type type;

      asset_id_type   asset_id;
      account_id_type account_id; // or balance ID
      bool            authorize = false;

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  This operation can only be performed by the *owner* of
    *  the issuing account.  Funds will be transferred into
    *  the to_account if the to_account is authorized to
    *  own the asset and the amount to issue would not
    *  exceed the maximum share supply.
    */
   struct issue_asset_operation
   {
       static const operation_type type;

       account_id_type to_account;
       asset           amount_to_issue;

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  This operation will publish a price feed and is only valid
    *  if the publisher is in the set of feed_producers or the 
    *  active set of delegates.  Published feeds are only
    *  valid for 24 hours.
    *
    *  A delegate can publish for free without having to
    *  sign only within their own block.
    */
   struct publish_asset_feed_operation
   {
       static const operation_type type;

       account_id_type publisher;
       price           asset_price;  // if price is 0, then price should follow base asset id

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    * Used by delegates to publish updates to the fee schedule.
    */
   struct public_fee_schedule_operation
   {
       static const operation_type type;
       
       delegate_id_type                                     delegate_id;
       vector< pair< enum_type<uint8_t,fee_type>, asset > > fee_schedule;
       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  Transfers an asset from one account to another account (optionally an authority).  
    *
    *  The from field must specify either a valid account *or* a valid balance ID.  
    *  A balance ID refers to a particular authority and asset type.  It is designed for
    *  operating without accounts and compatibility with other crypto-systems.  
    *
    *  The to field refers to either a valid account or balance ID or 0 if to_authority
    *  is set in which case a new authority will be defined. 
    *
    *  Some transfers of user issued assets require a fee to be paid to the issuer.  This
    *  fee will be taken from the "from account".
    */
   struct transfer_asset_operation
   {
       static const operation_type type;
       
       account_id_type     from; 
       account_id_type     to;  
       asset               amount;
       share_type          transfer_fee; // same as amount.asset_id
       /** By convention, uses the memo_key of the to account and  
        * from account to derive a one time child key of the to
        * account using the transaction timestamp and blockhash as
        * the seed. Then use AES to encrypt the memo.
        */
       vector<char>        memo; // MAX LENGTH 128 bytes (multiple of AES block size)
       
       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  Submits an order to be filled by the blockchain market
    *  engine.  It will trade the amount_for_sale for at
    *  least min_amount_to_receive if the total amount is sold.
    *
    *  The max_market_fee is used to protect the seller from the
    *  asset issuer changing the market fee while the order is open.
    *
    *  The result of this operation is the creation of a new order id
    *  controlled by from_account.
    */
   struct sell_asset_operation
   {
       static const operation_type type;
       
       account_id_type   from_account;
       asset             amount_for_sale;
       asset             min_amount_to_receive;
       time_point_sec    offer_expiration;
       uint16_t          max_market_fee;

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  This operation will cancel the full order and any
    *  remaining assets for sale will be returned to
    *  the owner.
    */
   struct cancel_sell_asset_operation
   {
       static const operation_type type;
       
       sell_order_id_type sell_order_id;

       object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  A short operation will take out a loan at the given APR interest 
    *  rate.   All short operations execute at the price feed. The short 
    *  seller can specifiy any call price above the minimum required call price
    *  to stop losses.  
    *
    *  A margin call is triggered when the price feed *AND* the highest bid is
    *  below the call price.  The collateral will be used to buy back and cover
    *  the full balance at any price up to 10% above the feed.  Don't get 
    *  margin called in a thin market. 
    *
    *
    *  
    */
   struct short_asset_operation
   {
      static const operation_type type;

      account_id_type from_account_id;
      asset           total_collateral;
      price           limit_price;  // protect the short from feed movements.
      price           call_price;   // desired call price, must be greater than min call price (2x)
      uint16_t        interest_rate_apr = 0; // 0 to 10,000 

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   struct update_cover_order_operation
   {
      static const operation_type type;

      account_id_type cover_account_id;
      cover_id_type   cover_order_id;
      asset           amount_to_cover; 
      asset           collateral_to_add;
      price           new_call_price;   // desired call price

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };


   /**
    *  All delegates must pay a basic registration fee to
    *  be considered.  This fee is to discourage the creation
    *  of frivious delegates that may dilute voter attention 
    *  and make certain blockchain operations more expensive.
    */
   struct register_delegate_operation
   {
      static const operation_type type;

      account_id_type   delegate_account;
      public_key_type   block_signing_key;
      uint16_t          pay_rate; ///< 0 to BTS_MAX_PAY_RATE 
      secret_hash_type  first_secret;
      vector<asset>     fee_schedule;
      asset             delegate_registration_fee;

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  All transactions must have this operation which specifies
    *  how much of a fee it will pay the network.  Network fees
    *  must be paid in BTS.  Light wallet service providers can
    *  optionally pay the network fee on behalf of users who
    *  pay the service provider in some other currency.
    *
    *  Asset issuers can also pay the fee on behalf of their
    *  users.
    */
   struct pay_fee_operation_operation
   {
      static const operation_type type;

      account_id_type paying_account; // or balance_id
      asset           fee;

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };


   /**
    *  Any one can publish an edge connecting any two objects
    *  in the network and specifying arbitrary data associated
    *  with the edge.  It is up to 3rd party applications to
    *  interpret the published edges.  For example shareholders
    *  could vote on a board approval by creating an edge between
    *  their account and the proposal data object.  
    */
   struct set_edge_operation
   {
      static const operation_type type;

      account_id_type updater; // account performing the update and paying fee
      edge_id_type    edge_id; // 0 to create a new edge
      string          from_name; // label on the from side. ie: Son
      object_id_type  from;
      string          to_name; // label on the to side.  ie: Father
      object_id_type  to;
      uint8_t         data_format;
      vector<char>    data;
      asset           data_fee;

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

   /**
    *  If data_object_id is 0 then create a new data object.  Fees
    *  are charged based upon how much data is stored on an edge.  The
    *  format of the data can be anything from binary, text, json, protobuf,
    *  to captain proto or a user defined format.  A data fee is charged
    *  on a per-byte basis for storing data with a 100 byte minimum.
    */
   struct set_data_object_operation
   {
      static const operation_type type;

      account_id_type updater; // account performing the update and paying fee
      object_id_type  data_object_id; // 0 to create a new data object
      uint8_t         data_format;
      vector<char>    data;
      asset           data_fee;

      object_id_type evaluate( transaction_evaluation_state& eval_state, bool apply = true );
   };

} }

FC_REFLECT( bts::chain::create_asset_operation, (symbol) )

FC_REFLECT( bts::chain::create_account_operation, 
            (name)
            (paying_account)
            (registration_fee)
            (owner)
            (active) 
            (voting)
          )

FC_REFLECT( bts::chain::update_asset_white_list_operation, (asset_id)(account_id)(authorize) )
FC_REFLECT( bts::chain::issue_asset_operation, (to_account)(amount_to_issue) )
FC_REFLECT( bts::chain::register_delegate_operation, 
            (delegate_account)
            (block_signing_key)
            (pay_rate)
            (first_secret)
            (fee_schedule)
            (delegate_registration_fee) )

FC_REFLECT( bts::chain::transfer_asset_operation, (from)(to)(amount)(transfer_fee)(memo) )
