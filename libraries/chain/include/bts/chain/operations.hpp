#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/authority.hpp>

namespace bts { namespace chain { 

   struct key_create_operation
   {
      account_id_type  fee_paying_account;
      asset            fee;
      static_variant<address,public_key_type> key_data;
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
   };

   struct asset_create_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct asset_update_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
   };

   struct delegate_create_operation
   {
      account_id_type fee_paying_account;
      asset           fee;
      account_id_type delegate_account;
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
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::asset_update_operation,
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::delegate_create_operation,
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::delegate_update_operation,
            (fee_paying_account)(fee) 
          )
FC_REFLECT( bts::chain::account_set_vote_operation,
            (fee_paying_account)(fee) 
          )

