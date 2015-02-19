#include <bts/chain/block.hpp>
#include <fc/io/raw.hpp>


namespace bts { namespace chain { 
   digest_type    block::digest()const
   {
      return fc::sha256::hash( *this );
   }

   block_id_type              signed_block::id()const
   {
      return fc::sha224::hash( *this );
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
