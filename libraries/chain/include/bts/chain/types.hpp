#pragma once
#include <fc/io/varint.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/optional.hpp>
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
   using                               std::set;
   using                               std::pair;
   using                               std::enable_shared_from_this;

   using                               fc::variant_object;
   using                               fc::enum_type;
   using                               fc::optional;
   using                               fc::unsigned_int;
   using                               fc::signed_int;
   using                               fc::time_point_sec;
   using                               fc::time_point;

   //typedef fc::unsigned_int            object_id_type;
   typedef uint32_t                    object_id_type;
   typedef object_id_type              account_id_type;
   typedef object_id_type              asset_id_type;
   typedef object_id_type              delegate_id_type;
   typedef fc::sha224                  block_id_type;
   typedef fc::sha256                  digest_type;
   typedef fc::ecc::compact_signature  signature_type;
   typedef int64_t                     share_type;
   typedef object_id_type              sell_order_id_type;
   typedef object_id_type              short_order_id_type;
   typedef object_id_type              cover_id_type;
   typedef object_id_type              edge_id_type;
   typedef fc::sha224                  secret_hash_type;

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
}
FC_REFLECT( bts::chain::public_key_type, (key_data) )
FC_REFLECT( bts::chain::public_key_type::binary_key, (data)(check) );
