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
      for( const auto& op : operations )
         op.visit(operation_validator());
   }

} } // bts::chain
