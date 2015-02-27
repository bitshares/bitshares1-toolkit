#include <bts/chain/key_object.hpp>
namespace bts { namespace chain {
   address key_object::key_address()const 
   { 
      typedef  static_variant<address,public_key_type> address_or_key;

      switch( key_data.which() )
      {
         case address_or_key::tag<address>::value:
            return key_data.get<address>();
         case address_or_key::tag<public_key_type>::value:
            return key_data.get<public_key_type>();
         default:
            assert( !"invalid state" );
      }
      return address();
   }
} } 
