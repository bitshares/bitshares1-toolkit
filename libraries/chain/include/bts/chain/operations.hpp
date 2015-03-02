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


      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct account_update_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct transfer_operation
   {
      account_id_type from;
      account_id_type to;
      asset           amount;
      share_type      fee; /// same asset_id as amount.asset_id
      vector<char>    memo;

      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_create_operation
   {
      account_id_type         issuer; // same as fee paying account
      asset                   fee;
      symbol_type             symbol;
      share_type              max_supply;
      uint16_t                market_fee_percent = 0;
      uint32_t                permissions = 0;
      uint32_t                flags = 0;
      price                   core_exchange_rate; // used for the fee pool
      vector<account_id_type> feed_producers; // for bitassets, specifies who produces the feeds (empty for delegates)
      asset_id_type           short_backing_asset; // for bitassets, specifies what may be used as collateral.

      share_type calculate_fee( const fee_schedule_type& k )const;
   };

   struct asset_update_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct delegate_create_operation
   {
      account_id_type                       delegate_account; // same as fee_paying account
      asset                                 fee;
      uint8_t                               pay_rate;  // 0 to 100% 
      secret_hash_type                      first_secret_hash;
      key_id_type                           signing_key;
      fc::array<share_type,FEE_TYPE_COUNT>  fee_schedule;
   };

   struct delegate_update_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct account_set_vote_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct custom_id_create_operation
   {
      account_id_type  owner;
   };

   struct custom_id_transfer_operation
   {
      custom_id_type   custom_id;
      account_id_type  new_owner;
   };

   struct custom_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
      uint16_t        type;
      vector<char>    data;
   };


   typedef fc::static_variant<
            transfer_operation,
            key_create_operation,
            account_create_operation,
            account_update_operation,
            delegate_create_operation,
            delegate_update_operation,
            asset_create_operation,
            asset_update_operation,
            account_set_vote_operation
         > operation;
      
} } // bts::chain

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
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::transfer_operation,
            (from)(to)(amount)(fee)(memo) )

FC_REFLECT( bts::chain::asset_create_operation,
            (issuer)
            (fee)
            (symbol)
            (max_supply)
            (market_fee_percent)
            (permissions)
            (flags)
            (core_exchange_rate)
            (feed_producers)
            (short_backing_asset) 
          )
FC_REFLECT( bts::chain::asset_update_operation,
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::delegate_create_operation,
            (delegate_account)(fee)(pay_rate)
            (first_secret_hash)(signing_key)
            (fee_schedule) 
          )
FC_REFLECT( bts::chain::delegate_update_operation,
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::account_set_vote_operation,
            (fee_paying_account)(fee) 
          )

