#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/address.hpp>

namespace bts { namespace chain {
   class key_object : public object
   {
      public:
         static const id_space_type space_id = protocol_ids;
         static const object_type   type_id  = key_object_type;
         address         key_address;
         public_key_type public_key;
   };
} }

FC_REFLECT_DERIVED( bts::chain::key_object, (bts::chain::object), (key_address)(public_key) )
