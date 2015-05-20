#include <bts/chain/operations.hpp>
#include <bts/chain/bond_object.hpp>
#include <bts/chain/vesting_balance_object.hpp>
#include <bts/chain/file_object.hpp>
#include <bts/chain/withdraw_permission_object.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/block.hpp>
#include <iostream>

using namespace bts::chain;

string remove_tail_if( const string& str, char c, const string& match )
{
   auto last = str.find_last_of( c );
   if( last != std::string::npos )
      if( str.substr( last + 1 ) == match )
         return str.substr( 0, last );
   return str;
}
string remove_namespace_if( const string& str, const string& match )
{
   auto last = str.find( match );
   if( last != std::string::npos )
      return str.substr( match.size()+2 );
   return str;
}


string remove_namespace( string str )
{
   str = remove_tail_if( str, '_', "operation" );
   str = remove_tail_if( str, '_', "t" );
   str = remove_tail_if( str, '_', "object" );
   str = remove_tail_if( str, '_', "type" );
   str = remove_namespace_if( str, "bts::chain" );
   str = remove_namespace_if( str, "bts::db" );
   str = remove_namespace_if( str, "std" );
   str = remove_namespace_if( str, "fc" );
   auto pos = str.find( ":" );
   if( pos != str.npos )
      str.replace( pos, 2, "_" );
   return str;
}




template<typename T>
void generate_serializer();
template<typename T> 
void register_serializer();


map<string, size_t >                st;
vector<std::function<void()>>       serializers;

bool register_serializer( const string& name, std::function<void()> sr )
{
   if( st.find(name) == st.end() )
   {
      serializers.push_back( sr );
      st[name] = serializers.size() - 1;
      return true;
   }
   return false;
}

template<typename T> struct js_name { static std::string name(){ return  remove_namespace(fc::get_typename<T>::name()); }; };

template<typename T, size_t N>
struct js_name<fc::array<T,N>>
{
   static std::string name(){ return  "fixed_array "+ fc::to_string(N) + ", "  + remove_namespace(fc::get_typename<T>::name()); };
};
template<size_t N>   struct js_name<fc::array<char,N>>    { static std::string name(){ return  "bytes "+ fc::to_string(N); }; };
template<size_t N>   struct js_name<fc::array<uint8_t,N>> { static std::string name(){ return  "bytes "+ fc::to_string(N); }; };
template<typename T> struct js_name< fc::optional<T> >    { static std::string name(){ return "optional " + js_name<T>::name(); } }; 
template<>           struct js_name< object_id_type >     { static std::string name(){ return "object_id_type"; } };
template<typename T> struct js_name< fc::flat_set<T> >    { static std::string name(){ return "set " + js_name<T>::name(); } };
template<typename T> struct js_name< std::vector<T> >     { static std::string name(){ return "array " + js_name<T>::name(); } };
template<typename T> struct js_name< fc::safe<T> > { static std::string name(){ return js_name<T>::name(); } };


template<> struct js_name< std::vector<char> > { static std::string name(){ return "bytes()";     } };
template<> struct js_name< op_wrapper >        { static std::string name(){ return "operation "; } };
template<> struct js_name<fc::uint160>         { static std::string name(){ return "bytes 20";   } };
template<> struct js_name<fc::sha224>          { static std::string name(){ return "bytes 28";   } };
template<> struct js_name<fc::unsigned_int>    { static std::string name(){ return "varuint32";  } };
template<> struct js_name<fc::signed_int>      { static std::string name(){ return "varint32";   } };
template<> struct js_name< vote_id_type >      { static std::string name(){ return "vote_id";    } };
template<> struct js_name< time_point_sec >    { static std::string name(){ return "uint32";     } };

template<uint8_t S, uint8_t T, typename O>
struct js_name<bts::db::object_id<S,T,O> >
{
   static std::string name(){ 
      return "protocol_id_type \"" + remove_namespace(fc::get_typename<O>::name()) + "\""; 
   };
};


template<typename T> struct js_name< std::set<T> > { static std::string name(){ return "set " + js_name<T>::name(); } };

template<typename K, typename V>
struct js_name< std::map<K,V> > { static std::string name(){ return "map (" + js_name<K>::name() + "), (" + js_name<V>::name() +")"; } };

template<typename K, typename V>
struct js_name< fc::flat_map<K,V> > { static std::string name(){ return "map (" + js_name<K>::name() + "), (" + js_name<V>::name() +")"; } };


template<typename... T> struct js_sv_name;

template<typename A> struct js_sv_name<A> 
{ static std::string name(){ return  "\n    " + js_name<A>::name(); } };

template<typename A, typename... T>
struct js_sv_name<A,T...> { static std::string name(){ return  "\n    " + js_name<A>::name() +"    " + js_sv_name<T...>::name(); } };


template<typename... T>
struct js_name< fc::static_variant<T...> >
{
   static std::string name( std::string n = ""){ 
      static const std::string name = n;
      if( name == "" )
         return "static_variant [" + js_sv_name<T...>::name() + "\n]"; 
      else return name;
   }
};


template<typename T, bool reflected = fc::reflector<T>::is_defined::value>
struct serializer;


struct register_type_visitor
{
   typedef void result_type;

   template<typename Type>
   result_type operator()( const Type& op )const { serializer<Type>::init(); }
};

class register_member_visitor;

struct serialize_type_visitor
{
   typedef void result_type;

   int t = 0;
   serialize_type_visitor(int _t ):t(_t){}

   template<typename Type>
   result_type operator()( const Type& op )const
   {
      std::cerr << "    " <<remove_namespace( fc::get_typename<Type>::name() )  <<": "<<t<<"\n";
   }
};


class serialize_member_visitor
{
   public:
      template<typename Member, class Class, Member (Class::*member)>
      void operator()( const char* name )const
      {
         std::cerr << "    " << name <<": " << js_name<Member>::name() <<"\n";
      }
};

template<typename T>
struct serializer<T,false>
{
   static_assert( fc::reflector<T>::is_defined::value == false, "invalid template arguments" );
   static void init()
   {}

   static void generate()
   {}
};

template<typename T, size_t N>
struct serializer<fc::array<T,N>,false>
{
   static void init() { serializer<T>::init(); }
   static void generate() {}
};
template<typename T>
struct serializer<std::vector<T>,false>
{
   static void init() { serializer<T>::init(); }
   static void generate() {}
};
template<>
struct serializer<std::vector<operation>,false>
{
   static void init() { }
   static void generate() {}
};

template<>
struct serializer<object_id_type,true>
{
   static void init() {}

   static void generate() {}
};
template<>
struct serializer<uint64_t,false>
{
   static void init() {}
   static void generate() {}
};
template<> struct serializer<vote_id_type,false> { static void init() {} static void generate() {} };
template<> struct serializer<size_t,false> { static void init() {} static void generate() {} };
template<> struct serializer<int64_t,false> { static void init() {} static void generate() {} };
template<> struct serializer<int64_t,true> { static void init() {} static void generate() {} };

template<typename T>
struct serializer<fc::optional<T>,false>
{
   static void init() { serializer<T>::init(); }
   static void generate(){}
};

template<uint8_t SpaceID, uint8_t TypeID, typename T>
struct serializer< bts::db::object_id<SpaceID,TypeID,T> ,true>
{
   static void init() {}
   static void generate() {}
};

template<typename... T>
struct serializer< fc::static_variant<T...>, false >
{
   static void init()
   {
      static bool init = false;
      if( !init )
      {
         init = true;
         fc::static_variant<T...> var;
         for( int i = 0; i < var.count(); ++i )
         {
            var.set_which(i);
            var.visit( register_type_visitor() );
         }
         register_serializer( js_name<fc::static_variant<T...>>::name(), [=](){ generate(); } );
      }
   }

   static void generate()
   {
      std::cerr <<  js_name<fc::static_variant<T...>>::name() << " = static_variant [" + js_sv_name<T...>::name() + "\n]\n\n"; 
   }
};

class register_member_visitor
{
   public:
      template<typename Member, class Class, Member (Class::*member)>
      void operator()( const char* name )const
      {
         serializer<Member>::init();
      }
};

template<typename T, bool reflected> 
struct serializer
{
   static_assert( fc::reflector<T>::is_defined::value == reflected, "invalid template arguments" );
   static void init()
   {
      auto name = js_name<T>::name();
      if( st.find(name) == st.end() )
      {
         fc::reflector<T>::visit( register_member_visitor() );
         register_serializer( name, [=](){ generate(); } );
      }
   }

   static void generate()
   {
      auto name = remove_namespace( js_name<T>::name() );
      if( name == "int64" ) return;
      std::cerr << "" << name
                << " = new Serializer( \n"
                << "    \"" + name + "\"\n";

      fc::reflector<T>::visit( serialize_member_visitor() );

      std::cerr <<")\n\n";
   }
};

int main( int argc, char** argv )
{
   try {
    operation op;

    std::cerr << "ChainTypes.operations=\n";
    for( uint32_t i = 0; i < op.count(); ++i )
    {
       op.set_which(i);
       op.visit( serialize_type_visitor(i) );
    }
    std::cerr << "\n";

    js_name<operation>::name("operation");
    js_name<static_variant<address,public_key_type>>::name("key_data");
    js_name<operation_result>::name("operation_result");
    js_name<static_variant<refund_worker_type::initializer, vesting_balance_worker_type::initializer>>::name("initializer_type");
    serializer<signed_block>::init();
    serializer<operation>::init();
    serializer<transaction>::init();
    serializer<signed_transaction>::init();
    for( const auto& gen : serializers )
       gen();

  } catch ( const fc::exception& e ){ edump((e.to_detail_string())); }
   return 0;
}
