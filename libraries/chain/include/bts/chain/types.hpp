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
#include <fc/string.hpp>
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
   using                               fc::static_variant;

   /**
    *
    */
   enum asset_issuer_permission_flags
   {
      charge_transfer_fee = 0x01,
      charge_market_fee   = 0x02,
      white_list          = 0x04,
      halt_market         = 0x08,
      halt_transfer       = 0x10,
      override_authority  = 0x20,
      market_issued       = 0x40
   };


   /**
    *  There are many types of fees charged by the network
    *  for different operations. These fees are published by
    *  the delegates and can change over time.
    */
   enum fee_type
   {
      key_create_fee_type,
      account_create_fee_type, ///< the cost to register the cheapest non-free account
      transfer_fee_type,
      asset_create_fee_type, ///< the cost to register the cheapest asset
      market_fee_type, ///< a percentage charged on market orders
      transaction_fee_type, ///< a base price for every transaction
      data_fee_type, ///< a price per 1024 bytes of user data
      delegate_registration_fee_type, ///< fixed fee for registering as a delegate, used to discourage frivioulous delegates
      signature_fee_type, ///< a surcharge on transactions with more than 2 signatures.

      FEE_TYPE_COUNT ///< Sentry value which contains the number of different fee types
   };


   struct object_id_type
   {
      object_id_type( uint8_t s, uint8_t t, uint64_t i )
      {
         assert( i>>48 == 0 );
         FC_ASSERT( i >> 48 == 0, "instance overflow", ("instance",i) );
         number = (uint64_t(s)<<56) | (uint64_t(t)<<48) | i;
      }
      object_id_type(){ number = 0; }

      uint8_t space()const       { return number >> 56;              }
      uint8_t type()const        { return number >> 48 & 0x00ff;     }
      uint16_t space_type()const { return number >> 48;              }
      uint8_t instance()const { return number & BTS_MAX_INSTANCE_ID; }
      bool    is_null()const { return number == 0; }
      operator uint64_t()const { return number; }

      friend bool  operator == ( const object_id_type& a, const object_id_type& b )
      {
         return a.number == b.number;
      }
      object_id_type& operator++(int) { ++number; return *this; }
      object_id_type& operator++()    { ++number; return *this; }

      uint64_t                   number;
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
      /** objects that may be referenced by other objects created as part of the same transaction */
      relative_protocol_ids = 0,
      /** objects that may be directly referred to by the protocol operations */
      protocol_ids          = 1,
      /** objects created for implementation specific reasons such as maximizing performance */
      implementation_ids    = 2,
      /** objects created for the purpose of tracking meta info not used by validation,
       * such as names and descriptions of assets or the value of data objects. */
      meta_info_ids = 3
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
      delegate_object_type,
      market_order_object_type,
      short_order_object_type,
      call_order_object_type,
      custom_object_type,
   };

   enum impl_object_type
   {
      impl_global_property_object_type,
      impl_index_meta_object_type,
      impl_asset_dynamic_data_type,
      impl_account_balance_object_type,
      impl_account_debt_object_type,
      impl_delegate_vote_object_type,
      impl_transaction_object_type
   };

   enum meta_info_object_type
   {
      meta_asset_object_type,
      meta_account_object_type
   };


   class object;
   class database;



   template<uint8_t SpaceID, uint8_t TypeID, typename T = object>
   struct object_id
   {
      typedef T type;
      static const uint8_t space_id = SpaceID;
      static const uint8_t type_id = TypeID;

      object_id(){}
      object_id( uint64_t i ):instance(i)
      {
         FC_ASSERT( (i >> 48) == 0 );
      }
      object_id( object_id_type id ):instance(id.instance())
      {
         assert( id.space() == SpaceID && id.type() == TypeID );
         FC_ASSERT( id.space() == SpaceID && id.type() == TypeID, "",
                    ("id.space",id.space())("SpaceID",SpaceID)
                    ("id.type",id.type())("TypeID",TypeID) );
      }

      operator object_id_type()const { return object_id_type( SpaceID, TypeID, instance.value ); }
      operator uint64_t()const { return object_id_type( *this ).number; }

      template<typename DB>
      const T* operator()(const DB& db)const { return db.get(*this); }

      friend bool  operator == ( const object_id& a, const object_id& b )
      {
         return a.instance == b.instance;
      }

      unsigned_int instance;
   };

   //typedef fc::unsigned_int            object_id_type;
   //typedef uint64_t                    object_id_type;
   class account_object;
   class delegate_object;
   class asset_object;
   class key_object;
   class market_order_object;
   class short_order_object;
   class call_order_object;
   class custom_object;

   typedef object_id< protocol_ids, key_object_type,          key_object>           key_id_type;
   typedef object_id< protocol_ids, account_object_type,      account_object>       account_id_type;
   typedef object_id< protocol_ids, asset_object_type,        asset_object>         asset_id_type;
   typedef object_id< protocol_ids, delegate_object_type,     delegate_object>      delegate_id_type;
   typedef object_id< protocol_ids, market_order_object_type, market_order_object>  market_order_id_type;
   typedef object_id< protocol_ids, short_order_object_type,  short_order_object>   short_order_id_type;
   typedef object_id< protocol_ids, call_order_object_type,   call_order_object>    call_order_id_type;
   typedef object_id< protocol_ids, custom_object_type,       custom_object>        custom_id_type;

   // implementation types
   class global_property_object;
   class index_meta_object;
   class delegate_vote_object;
   class asset_dynamic_data_object;
   class account_balance_object;
   class account_debt_object;
   class transaction_object;

   typedef object_id< implementation_ids, impl_global_property_object_type,  global_property_object>    global_property_id_type;
   typedef object_id< implementation_ids, impl_asset_dynamic_data_type,      asset_dynamic_data_object> dynamic_asset_data_id_type;
   typedef object_id< implementation_ids, impl_account_balance_object_type,  account_balance_object>    account_balance_id_type;
   typedef object_id< implementation_ids, impl_account_debt_object_type,     account_debt_object>       account_debt_id_type;
   typedef object_id< implementation_ids, impl_delegate_vote_object_type,    delegate_vote_object>      delegate_vote_id_type;
   typedef object_id< implementation_ids, impl_transaction_object_type,      transaction_object>        transaction_obj_id_type;


   typedef fc::sha224                                   block_id_type;
   typedef fc::sha256                                   digest_type;
   typedef fc::ecc::compact_signature                   signature_type;
   typedef safe<int64_t>                                share_type;
   typedef fc::sha224                                   secret_hash_type;
   typedef uint16_t                                     weight_type;


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

    inline void to_variant( const bts::chain::object_id_type& var,  fc::variant& vo )
    {
       vo = fc::to_string(var.space()) + "." + fc::to_string(var.type()) + "." + fc::to_string(var.instance());
    }
    inline void from_variant( const fc::variant& var,  bts::chain::object_id_type& vo )
    {
       vo.number = 0;
       const auto& s = var.get_string();
       auto first_dot = s.find('.');
       auto second_dot = s.find('.',first_dot+1);
       FC_ASSERT( first_dot != second_dot );
       FC_ASSERT( first_dot != 0 && first_dot != std::string::npos );
       vo.number = fc::to_uint64(s.substr( second_dot+1 ));
       FC_ASSERT( vo.number <= BTS_MAX_INSTANCE_ID );
       auto space_id = fc::to_uint64( s.substr( 0, first_dot ) );
       FC_ASSERT( space_id <= 0xff );
       auto type_id =  fc::to_uint64( s.substr( first_dot+1, second_dot ) );
       FC_ASSERT( type_id <= 0xff );
       vo.number |= (space_id << 56) | (type_id << 48);
    }
    template<uint8_t SpaceID, uint8_t TypeID, typename T>
    void to_variant( const bts::chain::object_id<SpaceID,TypeID,T>& var,  fc::variant& vo )
    {
       vo = fc::to_string(SpaceID) + "." + fc::to_string(TypeID) + "." + fc::to_string(var.instance.value);
    }
    template<uint8_t SpaceID, uint8_t TypeID, typename T>
    void from_variant( const fc::variant& var,  bts::chain::object_id<SpaceID,TypeID,T>& vo )
    {
       const auto& s = var.get_string();
       auto first_dot = s.find('.');
       auto second_dot = s.find('.',first_dot+1);
       FC_ASSERT( first_dot != second_dot );
       FC_ASSERT( first_dot != 0 && first_dot != std::string::npos );
       FC_ASSERT( fc::to_uint64( s.substr( 0, first_dot ) ) == SpaceID &&
                  fc::to_uint64( s.substr( first_dot+1, second_dot ) ) == TypeID );
       vo.instance = fc::to_uint64(s.substr( second_dot+1 ));
    }
}
FC_REFLECT( bts::chain::public_key_type, (key_data) )
FC_REFLECT( bts::chain::public_key_type::binary_key, (data)(check) );
FC_REFLECT( bts::chain::object_id_type, (number) )

// REFLECT object_id manually because it has 2 template params
namespace fc {
template<uint8_t SpaceID, uint8_t TypeID, typename T>
struct get_typename<bts::chain::object_id<SpaceID,TypeID,T>>
{
   static const char* name() {
      return typeid(get_typename).name();
      static std::string _str = string("bts::chain::object_id<")+fc::to_string(SpaceID) + ":" + fc::to_string(TypeID)+">";
      return _str.c_str();
   }
};

template<uint8_t SpaceID, uint8_t TypeID, typename T>
struct reflector<bts::chain::object_id<SpaceID,TypeID,T> >
{
    typedef bts::chain::object_id<SpaceID,TypeID,T> type;
    typedef fc::true_type  is_defined;
    typedef fc::false_type is_enum;
    enum  member_count_enum {
      local_member_count = 1,
      total_member_count = 1
    };
    template<typename Visitor>
    static inline void visit( const Visitor& visitor )
    {
       typedef decltype(((type*)nullptr)->instance) member_type;
       visitor.TEMPLATE operator()<member_type,type,&type::instance>( "instance" );
    }
};
} // namespace fc



FC_REFLECT_ENUM( bts::chain::id_space_type, (relative_protocol_ids)(protocol_ids)(implementation_ids)(meta_info_ids) )
FC_REFLECT_ENUM( bts::chain::object_type,
                 (null_object_type)
                 (base_object_type)
                 (key_object_type)
                 (account_object_type)
                 (asset_object_type)
                 (delegate_object_type)
                 (market_order_object_type)
                 (short_order_object_type)
                 (call_order_object_type)
                 (custom_object_type)
               )
FC_REFLECT_ENUM( bts::chain::impl_object_type,
                 (impl_global_property_object_type)
                 (impl_index_meta_object_type)
                 (impl_asset_dynamic_data_type)
                 (impl_account_balance_object_type)
                 (impl_account_debt_object_type)
                 (impl_delegate_vote_object_type)
                 (impl_transaction_object_type)
               )

FC_REFLECT_ENUM( bts::chain::meta_info_object_type, (meta_account_object_type)(meta_asset_object_type) )
