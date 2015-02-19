#pragma once
#include <fc/io/raw.hpp>

namespace bts { namespace chain {
   struct packed_object
   {
      enum_type<uint16_t,object_type>  type = null_object_type;
      vector<char>                     data;
      
      packed_object(){}

      template<typename T>
      packed_object( const T& o )
      {
         type = o.type;
         data = fc::raw::pack( o );
      }

      template<typename T>
      void unpack( T& o ) 
      { 
         FC_ASSERT( o.type == type ); 
         fc::raw::unpack( data, o ); 
      }
   };
} }
