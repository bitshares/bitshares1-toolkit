#pragma once
#include <fc/container/flat_fwd.hpp>
#include <fc/io/varint.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/optional.hpp>
#include <fc/safe.hpp>
#include <fc/container/flat.hpp>
#include <memory>
#include <vector>
#include <deque>
#include <bts/chain/address.hpp>

namespace bts { namespace chain {
   using                               std::map;
   using                               std::vector;
   using                               std::unordered_map;
   using                               std::string;
   using                               std::deque;
   using                               std::shared_ptr;
   using                               std::unique_ptr;
   using                               std::set;
   using                               std::pair;
   using                               std::enable_shared_from_this;
   using                               std::tie;
   using                               std::make_pair;

   using                               fc::variant_object;
   using                               fc::variant;
   using                               fc::enum_type;
   using                               fc::optional;
   using                               fc::unsigned_int;
   using                               fc::signed_int;
   using                               fc::time_point_sec;
   using                               fc::time_point;
   using                               fc::safe;
   using                               fc::flat_map;
   using                               fc::flat_set;

   struct object_id_bits
   {
      uint64_t space     : 8;
      uint64_t type      : 8;
      uint64_t instance  : 48;
   };
   struct object_id_space_type_bits
   {
      uint64_t space_type : 16;
      uint64_t instance   : 48;
   };
   union object_id_type
   {
      object_id_type( uint8_t s, uint8_t t, uint64_t i )
      {
         bits.space = s;
         bits.type = t;
         bits.instance = i;
      }
      object_id_type(){ number = 0; }
   
      uint8_t space()const { return bits.space; }
      uint8_t type()const { return bits.type; }
      uint16_t space_type()const { return space_type_bits.space_type; }
      uint8_t instance()const { return bits.instance; }
      bool    is_null()const { return number == 0; }
      operator uint64_t()const { return number; }

      friend bool  operator == ( const object_id_type& a, const object_id_type& b )
      {
         return a.number == b.number;
      }
      object_id_type& operator++(int) { bits.instance++; return *this; }
      object_id_type& operator++()    { bits.instance++; return *this; }

      uint64_t                   number;
      object_id_bits             bits;
      object_id_space_type_bits  space_type_bits;
   };

   /**
    *  Objects are divided into namespaces each with 
    *  their own unique sequence numbers for both
    *  object IDs and types.  These namespaces
    *  are useful for building plugins that wish
    *  to leverage the same ID infrastructure.
    */
   enum id_space_type
   {
      /** objects that may be directly referred to by the protocol operations */
      protocal_ids = 0,
      /** objects created for implementation specific reasons such as maximizing performance */
      implementation_ids = 1,
      /** objects created for the purpose of tracking meta info not used by validation, 
       * such as names and descriptions of assets or the value of data objects. */
      meta_info_ids = 2
   };


   /**
    *  List all object types from all namespaces here so they can
    *  be easily reflected and displayed in debug output.  If a 3rd party
    *  wants to extend the core code then they will have to change the
    *  packed_object::type field from enum_type to uint16 to avoid
    *  warnings when converting packed_objects to/from json.
    */
   enum object_type
   {
      null_object_type,
      base_object_type,
      key_object_type,
      account_object_type,
      asset_object_type,
      delegate_object_type               
   };

   enum impl_object_type
   {
      impl_index_meta_object_type,
      impl_account_balance_object_type,
      impl_delegate_vote_object_type
   };

   enum meta_info_object_type
   {
      meta_asset_object_type 
   };


   template<uint16_t SpaceTypeID>
   struct object_id
   {
      object_id(){}
      object_id( uint64_t i ):instance(i)
      {
         FC_ASSERT( (i >> 48) == 0 );
      }
      object_id( object_id_type id ):instance(id.instance())
      {
         FC_ASSERT( id.space_type() == SpaceTypeID );
      }
      operator object_id_type()const { return object_id_type( SpaceTypeID>>8, SpaceTypeID&0x00ff, instance.value ); }
      operator uint64_t()const { return object_id_type( *this ).number; }

      friend bool  operator == ( const object_id& a, const object_id& b )
      {
         return a.instance == b.instance;
      }

      unsigned_int instance;
   };

   //typedef fc::unsigned_int            object_id_type;
   //typedef uint64_t                    object_id_type;
   typedef object_id< (protocal_ids<<8) | account_object_type>   account_id_type;
   typedef object_id< (protocal_ids<<8) | asset_object_type>     asset_id_type;
   typedef object_id< (protocal_ids<<8) | delegate_object_type>  delegate_id_type;
   typedef fc::sha224                                   block_id_type;
   typedef fc::sha256                                   digest_type;
   typedef fc::ecc::compact_signature                   signature_type;
   typedef safe<int64_t>                                share_type;
   typedef object_id_type                               sell_order_id_type;
   typedef object_id_type                               short_order_id_type;
   typedef object_id_type                               cover_id_type;
   typedef object_id_type                               edge_id_type;
   typedef fc::sha224                                   secret_hash_type;
   typedef uint16_t                                     weight_type;

   class account_object;
   class delegate_object;
   class asset_object;
   class balance_object;

   struct public_key_type
   {
       struct binary_key
       {
          binary_key():check(0){}
          uint32_t                 check;
          fc::ecc::public_key_data data;
       };

       fc::ecc::public_key_data key_data;

       public_key_type();
       public_key_type( const fc::ecc::public_key_data& data );
       public_key_type( const fc::ecc::public_key& pubkey );
       explicit public_key_type( const std::string& base58str );
       operator fc::ecc::public_key_data() const;
       operator fc::ecc::public_key() const;
       explicit operator std::string() const;
       friend bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2);
       friend bool operator == ( const public_key_type& p1, const public_key_type& p2);
       friend bool operator != ( const public_key_type& p1, const public_key_type& p2);
   };

} }  // bts::chain

namespace fc
{
    void to_variant( const bts::chain::public_key_type& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  bts::chain::public_key_type& vo );
    template<uint8_t SpaceTypeID>
    void to_variant( const bts::chain::object_id<SpaceTypeID>& var,  fc::variant& vo ) { to_variant( var.instance, vo ); }
    template<uint8_t SpaceTypeID>
    void from_variant( const fc::variant& var,  bts::chain::object_id<SpaceTypeID>& vo ) { from_variant( var, vo.instance ); }
}
FC_REFLECT( bts::chain::public_key_type, (key_data) )
FC_REFLECT( bts::chain::public_key_type::binary_key, (data)(check) );
FC_REFLECT( bts::chain::object_id_type, (number) )
FC_REFLECT_TEMPLATE( (uint16_t SpaceTypeID), bts::chain::object_id<SpaceTypeID>, (instance) )

FC_REFLECT_ENUM( bts::chain::id_space_type, (protocal_ids)(implementation_ids)(meta_info_ids) )
FC_REFLECT_ENUM( bts::chain::object_type,
                 (null_object_type)
                 (base_object_type)
                 (key_object_type)
                 (account_object_type) 
                 (asset_object_type)
                 (delegate_object_type)
               )
FC_REFLECT_ENUM( bts::chain::impl_object_type, 
                 (impl_index_meta_object_type)
                 (impl_account_balance_object_type)
                 (impl_delegate_vote_object_type) )
