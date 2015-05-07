#include <bts/chain/transaction.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace chain {

   digest_type transaction::digest()const
   {
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
      auto hash = digest();
      transaction_id_type result;
      memcpy(result._hash, hash._hash, std::min(sizeof(result), sizeof(hash)));
      return result;
   }
   void bts::chain::signed_transaction::sign( key_id_type id, const private_key_type& key )
   {
      signatures[id] =  key.sign_compact( digest() );
   }

} } // bts::chain
