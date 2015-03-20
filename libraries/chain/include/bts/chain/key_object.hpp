#pragma once
#include <bts/db/object.hpp>
#include <bts/chain/address.hpp>
#include <fc/static_variant.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {
   class key_object : public bts::db::abstract_object<key_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = key_object_type;

         key_id_type get_id()const  { return key_id_type( id.instance() ); }
         address key_address()const;
         const public_key_type& key()const { return key_data.get<public_key_type>(); }

         static_variant<address,public_key_type> key_data;
   };
} }

FC_REFLECT_DERIVED( bts::chain::key_object, (bts::db::object), (key_data) )
