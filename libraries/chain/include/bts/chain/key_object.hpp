#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/address.hpp>

namespace bts { namespace chain {
   class key_object : public object
   {
      public:
         static const object_type type = key_object_type;
         address    key_address;
   };
} }

FC_REFLECT_DERIVED( bts::chain::key_object, (bts::chain::object), (key_address) )
