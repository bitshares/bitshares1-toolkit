#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/transaction.hpp>

namespace bts { namespace chain {

   struct block_header
   {
      digest_type                   digest()const;
      block_id_type                 previous;
      uint32_t                      block_num()const { return num_from_id(previous) + 1; }
      fc::time_point_sec            timestamp;
      witness_id_type               witness;
      secret_hash_type              next_secret_hash;
      secret_hash_type              previous_secret;
      checksum_type                 transaction_merkle_root;

      static uint32_t num_from_id(const block_id_type& id) { return htonl(id._hash[0]); }
   };

   struct signed_block_header : public block_header
   {
      block_id_type              id()const;
      fc::ecc::public_key        signee()const;
      void                       sign( const fc::ecc::private_key& signer );
      bool                       validate_signee( const fc::ecc::public_key& expected_signee )const;

      signature_type             delegate_signature;
   };

   struct signed_block : public signed_block_header
   {
      checksum_type calculate_merkle_root()const;
      vector<processed_transaction> transactions;
   };

} } // bts::chain

FC_REFLECT( bts::chain::block_header, (previous)(timestamp)(witness)
            (next_secret_hash)(previous_secret)(transaction_merkle_root) )
FC_REFLECT_DERIVED( bts::chain::signed_block_header, (bts::chain::block_header), (delegate_signature) )
FC_REFLECT_DERIVED( bts::chain::signed_block, (bts::chain::signed_block_header), (transactions) )
