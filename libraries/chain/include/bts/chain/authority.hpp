#pragma once
#include <fc/io/varint.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct authority
   {
      fc::unsigned_int required;
      vector<public_key_type> keys;
   };

   struct address_authority
   {
      address_authority(){}
      address_authority( const address& single ):required(1),addresses({single}){}
      address_authority( const authority& a )
      :required(a.required)
      { 
         addresses.reserve( a.keys.size() );
         for( auto key : a.keys ) addresses.push_back( key );
      }

      fc::unsigned_int required;
      vector<address> addresses;
   };

} } // namespace bts::chain

FC_REFLECT( bts::chain::authority, (required)(keys)  )
FC_REFLECT( bts::chain::address_authority, (required)(addresses)  )
