#include <bts/chain/block.hpp>
#include <fc/io/raw.hpp>


namespace bts { namespace chain {
   digest_type    block_header::digest()const
   {
      return digest_type::hash(*this);
   }

   block_id_type              signed_block_header::id()const
   {
      auto tmp = fc::sha224::hash( *this );
      tmp._hash[0] = htonl(block_num()); // store the block num in the ID, 160 bits is plenty for the hash
      static_assert( sizeof(tmp._hash[0]) == 4, "should be 4 bytes" );
      block_id_type result;
      memcpy(result._hash, tmp._hash, std::min(sizeof(result), sizeof(tmp)));
      return result;
   }

   fc::ecc::public_key        signed_block_header::signee()const
   {
      return fc::ecc::public_key( delegate_signature, digest(), true/*enforce canonical*/ );
   }

   void                       signed_block_header::sign( const fc::ecc::private_key& signer )
   {
      delegate_signature = signer.sign_compact( digest() );
   }

   bool                       signed_block_header::validate_signee( const fc::ecc::public_key& expected_signee )const
   {
      return signee() == expected_signee;
   }

   checksum_type  signed_block::calculate_merkle_root()const
   {
      if( transactions.size() == 0 ) return checksum_type();

      vector<digest_type>  ids;
      ids.resize( ((transactions.size() + 1)/2)*2 );
      for( uint32_t i = 0; i < transactions.size(); ++i )
         ids[i] = transactions[i].merkle_digest();

      while( ids.size() > 1 )
      {
         for( uint32_t i = 0; i < transactions.size(); i += 2 )
            ids[i/2] = digest_type::hash( std::make_pair( ids[i], ids[i+1] ) );
         ids.resize( ids.size() / 2 );
      }
      return checksum_type::hash( ids[0] );
   }

} }
