#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/transaction.hpp>

namespace bts { namespace chain {

   struct block
   {
      digest_type                   digest()const;

      block_id_type                 previous;
      fc::time_point_sec            timestamp;
      uint32_t                      block_num = 0;
      delegate_id_type              delegate_id;
      secret_hash_type              next_secret_hash;
      secret_hash_type              previous_secret;
      vector<processed_transaction> transactions;
   };

   struct signed_block : public block
   {
      block_id_type              id()const;
      fc::ecc::public_key        signee()const;
      void                       sign( const fc::ecc::private_key& signer );
      bool                       validate_signee( const fc::ecc::public_key& expected_signee )const;

      signature_type             delegate_signature;
   };

} } // bts::chain

FC_REFLECT( bts::chain::block, (previous)(timestamp)(block_num)(delegate_id)
            (next_secret_hash)(previous_secret)(transactions) )
FC_REFLECT_DERIVED( bts::chain::signed_block, (bts::chain::block), (delegate_signature) )

