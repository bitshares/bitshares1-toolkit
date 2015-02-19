#pragma once
#include <bts/chain/types.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace chain {

   enum object_type
   {
      null_object_type              = 0,
      base_object_type              ,
      account_object_type           ,
      account_balance_object_type   ,
      asset_object_type             ,
      delegate_object_type          ,
      delegate_vote_object_type     ,
      balance_object_type           ,
      object_type_count
   };

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
      void unpack( T& o )const
      { 
         FC_ASSERT( o.type == type ); 
         fc::raw::unpack( data, o ); 
      }
   };

   class object 
   {
      public:
         object( object_type t = base_object_type ):type(t){};
         virtual ~object(){};

         uint64_t object_id()const    { return id & 0x7fffffffffffffff; }
         bool     is_dirty()const     { return id & 0x8000000000000000; }
         void     mark_dirty()        { id |= 0x8000000000000000;       }

         const enum_type<uint16_t,object_type> type;
         object_id_type                        id = 0;
   };

} }

FC_REFLECT_ENUM( bts::chain::object_type,
                 (null_object_type)
                 (base_object_type)
                 (account_object_type) 
                 (account_balance_object_type) 
                 (asset_object_type)
                 (delegate_object_type)
                 (delegate_vote_object_type)
                 (balance_object_type)
                 (object_type_count)
               )

FC_REFLECT( bts::chain::object, (type)(id) )
