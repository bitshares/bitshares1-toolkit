#pragma once
#include <bts/chain/config.hpp>
#include <fc/io/varint.hpp>

namespace bts { namespace chain {
   using                               fc::unsigned_int;
   using                               fc::signed_int;

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

   struct object_id_type
   {
      object_id_type( uint8_t s, uint8_t t, uint64_t i )
      {
         assert( i>>48 == 0 );
         FC_ASSERT( i >> 48 == 0, "instance overflow", ("instance",i) );
         number = (uint64_t(s)<<56) | (uint64_t(t)<<48) | i;
      }
      object_id_type(){ number = 0; }

      bool     is_relative()const { return space() == relative_protocol_ids; }
      uint8_t  space()const       { return number >> 56;              }
      uint8_t  type()const        { return number >> 48 & 0x00ff;     }
      uint16_t space_type()const { return number >> 48;              }
      uint64_t instance()const { return number & BTS_MAX_INSTANCE_ID; }
      bool     is_null()const { return number == 0; }
      operator uint64_t()const { return number; }

      friend bool  operator == ( const object_id_type& a, const object_id_type& b )
      {
         return a.number == b.number;
      }
      object_id_type& operator++(int) { ++number; return *this; }
      object_id_type& operator++()    { ++number; return *this; }

      friend size_t hash_value( object_id_type v ) { return std::hash<uint64_t>()(v.number); }

      friend bool  operator < ( const object_id_type& a, const object_id_type& b )
      {
         return a.number < b.number;
      }

      uint64_t                   number;
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
         assert( (id.space() == SpaceID && id.type() == TypeID)
                 || id.space() == relative_protocol_ids );
      }
      bool is_relative()const { return SpaceID == relative_protocol_ids; }

      operator object_id_type()const { return object_id_type( SpaceID, TypeID, instance.value ); }
      operator uint64_t()const { return object_id_type( *this ).number; }

      template<typename DB>
      const T& operator()(const DB& db)const { return db.get(*this); }

      friend bool  operator == ( const object_id& a, const object_id& b )
      {
         return a.instance == b.instance;
      }
      friend bool  operator == ( const object_id_type& a, const object_id& b )
      {
         return a == object_id_type(b);
      }
      friend bool  operator == ( const object_id& b, const object_id_type& a )
      {
         return a == object_id_type(b);
      }
      friend bool  operator < ( const object_id& a, const object_id& b )
      {
         return a.instance.value < b.instance.value;
      }

      unsigned_int instance;
   };

   template<uint8_t TypeID, typename T>
   struct object_id<0,TypeID,T>
   {
      typedef T type;
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = TypeID;

      object_id(){}
      object_id( uint64_t i ):instance(i)
      {
         FC_ASSERT( (i >> 48) == 0 );
      }
      object_id( object_id_type id ):instance(id.instance())
      {
         assert( (id.type() == TypeID)
                 || id.space() == relative_protocol_ids );
      }

      bool     is_relative()const { return instance.value < 0; }
      uint64_t relative_id()const { return llabs( instance.value ); }

      operator object_id_type()const
      {
         if( is_relative() ) return object_id_type( 0, 0, llabs(instance.value) );
         else return object_id_type( protocol_ids, TypeID, instance.value );
      }
      operator uint64_t()const { return object_id_type( *this ).number; }

      template<typename DB>
      const T& operator()(const DB& db)const { FC_ASSERT( !is_relative() ); return db.get(*this); }

      friend bool  operator == ( const object_id_type& a, const object_id& b )
      {
         return object_id(a).instance == b.instance;
      }

      friend bool  operator == ( const object_id& a, const object_id& b )
      {
         return a.instance == b.instance;
      }
      friend bool  operator < ( const object_id& a, const object_id& b )
      {
         return a.instance.value < b.instance.value;
      }

      signed_int instance;
   };

} } // bts::chain

FC_REFLECT_ENUM( bts::chain::id_space_type, (relative_protocol_ids)(protocol_ids)(implementation_ids)(meta_info_ids) )
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


 inline void to_variant( const bts::chain::object_id_type& var,  fc::variant& vo )
 {
    vo = fc::to_string(var.space()) + "." + fc::to_string(var.type()) + "." + fc::to_string(var.instance());
 }
 inline void from_variant( const fc::variant& var,  bts::chain::object_id_type& vo )
 { try {
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
    auto type_id =  fc::to_uint64( s.substr( first_dot+1, second_dot-first_dot-1 ) );
    FC_ASSERT( type_id <= 0xff );
    vo.number |= (space_id << 56) | (type_id << 48);
 } FC_CAPTURE_AND_RETHROW( (var) ) }
 template<uint8_t SpaceID, uint8_t TypeID, typename T>
 void to_variant( const bts::chain::object_id<SpaceID,TypeID,T>& var,  fc::variant& vo )
 {
    vo = fc::to_string(SpaceID) + "." + fc::to_string(TypeID) + "." + fc::to_string(var.instance.value);
 }
 template<uint8_t SpaceID, uint8_t TypeID, typename T>
 void from_variant( const fc::variant& var,  bts::chain::object_id<SpaceID,TypeID,T>& vo )
 { try {
    const auto& s = var.get_string();
    auto first_dot = s.find('.');
    auto second_dot = s.find('.',first_dot+1);
    FC_ASSERT( first_dot != second_dot );
    FC_ASSERT( first_dot != 0 && first_dot != std::string::npos );
    FC_ASSERT( fc::to_uint64( s.substr( 0, first_dot ) ) == SpaceID &&
               fc::to_uint64( s.substr( first_dot+1, second_dot-first_dot-1 ) ) == TypeID );
    vo.instance = fc::to_uint64(s.substr( second_dot+1 ));
 } FC_CAPTURE_AND_RETHROW( (var) ) }

} // namespace fc

namespace std {
     template <> struct hash<bts::chain::object_id_type>
     {
          size_t operator()(const bts::chain::object_id_type& x) const
          {
              return std::hash<uint64_t>()(x.number);
          }
     };
}
