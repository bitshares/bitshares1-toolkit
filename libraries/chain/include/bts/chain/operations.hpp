#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain {

   bool is_valid_symbol( const string& symbol );
   bool is_valid_name( const string& s );
   bool is_premium_name( const string& n );
   bool is_cheap_name( const string& n );

   struct key_create_operation
   {
      account_id_type  fee_paying_account;
      asset            fee;
      static_variant<address,public_key_type> key_data;

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

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct account_publish_feeds_operation
   {
      account_id_type   account;
      asset             fee; ///< paid for by account
      flat_set<price>   feeds; ///< must be sorted with no duplicates

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

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_update_operation
   {
      asset_id_type   asset_to_update;
      asset           fee; ///< paid by asset_to_update->issuer

      uint16_t         flags = 0;
      uint16_t         permissions = 0;
      optional<price>  core_exchange_rate;

      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_issue_operation
   {
      asset            asset_to_issue;
      asset            fee; ///< paid by asset_to_issue->asset_id->issuer
      account_id_type  issue_to_account;

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
      fc::array<share_type,FEE_TYPE_COUNT>  fee_schedule;

      void validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct delegate_update_operation
   {
      delegate_id_type                                delegate_id;
      asset                                           fee; ///< paid by delegate_id->delegate_account
      optional<fc::array<share_type,FEE_TYPE_COUNT>>  fee_schedule;
      optional<relative_key_id_type>                  signing_key;
      uint8_t                                         pay_rate; ///< 255 for unchanged
      uint8_t                                         block_interval_sec = BTS_DEFAULT_BLOCK_INTERVAL; 
      uint32_t                                        max_block_size = BTS_DEFAULT_MAX_BLOCK_SIZE; 
      uint32_t                                        max_transaction_size = BTS_DEFAULT_MAX_TRANSACTION_SIZE; 
      uint32_t                                        max_sec_until_expiration = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION; 

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct custom_id_create_operation
   {
      account_id_type  owner;

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct custom_id_transfer_operation
   {
      custom_id_type   custom_id;
      account_id_type  new_owner;

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct custom_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
      uint16_t        type;
      vector<char>    data;

      void       validate()const;
      share_type calculate_fee( const fee_schedule_type& k )const;
   };


   /** op_wrapper is used to get around the circular
    * definition of operation and proposals that contain
    * them.
    */
   struct op_wrapper;
   struct proposal_create_operation
   {
       account_id_type    fee_paying_account;
       asset              fee;
       vector<op_wrapper> proposed_ops;

      void       validate()const {}
      share_type calculate_fee( const fee_schedule_type& k )const { return 0; }
   };


   typedef fc::static_variant<
            transfer_operation,
            key_create_operation,
            account_create_operation,
            account_update_operation,
            account_publish_feeds_operation,
            delegate_create_operation,
            delegate_update_operation,
            asset_create_operation,
            asset_update_operation,
            asset_issue_operation,
            proposal_create_operation
         > operation;

   /**
    * @brief Performs default validation / sanity checks on operations that do not depend upon blockchain state.
    */
   struct operation_validator
   {
      typedef void result_type;
      template<typename T>
      void operator()( const T& v )const { v.validate(); }
   };

   /**
    * @brief Used to calculate fees in a polymorphic manner
    */
   struct operation_calculate_fee
   {
      const fee_schedule_type& fees;
      operation_calculate_fee( const fee_schedule_type& f ):fees(f){}
      typedef share_type result_type;
      template<typename T>
      share_type operator()( const T& v )const { return v.calculate_fee(fees); }
   };
   /**
    * @brief Used to set fees in a polymorphic manner
    */
   struct operation_set_fee
   {
      const fee_schedule_type& fees;
      operation_set_fee( const fee_schedule_type& f ):fees(f){}
      typedef asset result_type;
      template<typename T>
      asset operator()( T& v )const { return v.fee = asset(v.calculate_fee(fees)); }
   };

   struct op_wrapper
   {
      public:
      operation op;

      void       validate()const { op.visit( operation_validator() ); }
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

FC_REFLECT( bts::chain::account_publish_feeds_operation,
            (account)(fee)(feeds) )

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
            (asset_to_update)(fee)(flags)(permissions)(core_exchange_rate)
          )

FC_REFLECT( bts::chain::asset_issue_operation,
            (asset_to_issue)(fee)(issue_to_account) )

FC_REFLECT( bts::chain::delegate_create_operation,
            (delegate_account)(fee)(pay_rate)
            (first_secret_hash)(signing_key)
            (fee_schedule)
          )

FC_REFLECT( bts::chain::delegate_update_operation,
            (delegate_id)(fee)(fee_schedule)(signing_key)(pay_rate)
          )

FC_REFLECT( bts::chain::proposal_create_operation, (fee_paying_account)(fee)(proposed_ops) )

