#include <bts/chain/block.hpp>
#include <fc/io/raw.hpp>


namespace bts { namespace chain {
   digest_type    block::digest()const
   {
      return digest_type::hash(*this);
   }

   block_id_type              signed_block::id()const
   {
      auto tmp = fc::sha224::hash( *this );
      tmp._hash[0] = htonl(block_num()); // store the block num in the ID, 160 bits is plenty for the hash
      static_assert( sizeof(tmp._hash[0]) == 4, "should be 4 bytes" );
      block_id_type result;
      memcpy(result._hash, tmp._hash, std::min(sizeof(result), sizeof(tmp)));
      return result;
   }

   fc::ecc::public_key        signed_block::signee()const
   {
      return fc::ecc::public_key( delegate_signature, digest(), true/*enforce canonical*/ );
   }

   void                       signed_block::sign( const fc::ecc::private_key& signer )
   {
      delegate_signature = signer.sign_compact( digest() );
   }

   bool                       signed_block::validate_signee( const fc::ecc::public_key& expected_signee )const
   {
      return signee() == expected_signee;
   }

} }
