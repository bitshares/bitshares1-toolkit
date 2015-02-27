#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/address.hpp>

namespace bts { namespace chain {
   class key_object : public object
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = key_object_type;

         key_id_type get_id()const  { return key_id_type( id.instance() ); }
         address key_address()const;

         static_variant<address,public_key_type> key_data;
   };
} }

FC_REFLECT_DERIVED( bts::chain::key_object, (bts::chain::object), (key_data) )
