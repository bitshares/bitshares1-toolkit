#pragma once
#include <fc/io/varint.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct authority
   {
      /** this is the minimum signatures required, must be >= abs_required */
      fc::unsigned_int        min_required;
      /** the first N are absolutely required regardless of number of signatures */
      fc::unsigned_int        abs_required;
      vector<public_key_type> keys;
   };

   struct address_authority
   {
      address_authority(){}
      address_authority( const address& single ):min_required(1),abs_required(1),addresses({single}){}
      address_authority( const authority& a )
      :min_required(a.min_required),abs_required(a.abs_required)
      { 
         addresses.reserve( a.keys.size() );
         for( auto key : a.keys ) addresses.push_back( key );
      }

      /** this is the minimum signatures required, must be >= abs_required */
      fc::unsigned_int        min_required;
      /** the first N are absolutely required regardless of number of signatures */
      fc::unsigned_int        abs_required;
      vector<address> addresses;
   };

} } // namespace bts::chain

FC_REFLECT( bts::chain::authority, (min_required)(abs_required)(keys)  )
FC_REFLECT( bts::chain::address_authority, (min_required)(abs_required)(addresses)  )
