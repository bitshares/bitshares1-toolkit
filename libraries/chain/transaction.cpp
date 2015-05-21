#include <bts/chain/transaction.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace chain {

digest_type transaction::digest(const block_id_type& ref_block_id) const
{
   digest_type::encoder enc;
   fc::raw::pack( enc, ref_block_id );
   fc::raw::pack( enc, *this );
   return enc.result();
}

digest_type processed_transaction::merkle_digest()const
{
   return digest_type::hash(*this);
}

digest_type transaction::digest()const
{
   //Only use this digest() for transactions with absolute expiration times.
   if( relative_expiration != 0 ) edump((*this));
   assert(relative_expiration == 0);
   digest_type::encoder enc;
   fc::raw::pack( enc, *this );
   return enc.result();
}
void transaction::validate() const
{
   if( relative_expiration == 0 )
      FC_ASSERT( ref_block_num == 0 && ref_block_prefix > 0 );

   for( const auto& op : operations )
      op.visit(operation_validator());
}

bts::chain::transaction_id_type bts::chain::transaction::id() const
{
   digest_type::encoder enc;
   fc::raw::pack(enc, *this);
   auto hash = enc.result();
   transaction_id_type result;
   memcpy(result._hash, hash._hash, std::min(sizeof(result), sizeof(hash)));
   return result;
}
void bts::chain::signed_transaction::sign( key_id_type id, const private_key_type& key )
{
   if( relative_expiration != 0 )
   {
      if( !block_id_cache.valid() ) edump((*this));
      assert(block_id_cache.valid());
      signatures[id] =  key.sign_compact( digest(*block_id_cache) );
   } else {
      signatures[id] =  key.sign_compact( digest() );
   }
}

} } // bts::chain
