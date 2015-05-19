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


string remove_namespace( string str )
{
   str = remove_tail_if( str, '_', "operation" );
   str = remove_tail_if( str, '_', "t" );
   str = remove_tail_if( str, '_', "object" );
   str = remove_tail_if( str, '_', "type" );
   auto last = str.find_last_of( ':' );
   if( last != std::string::npos )
      return str.substr( last + 1 );
   else
      return str;
}

template<typename T>
struct js_name
{
   static std::string name(){ return "st." + remove_namespace(fc::get_typename<T>::name()); };
};

template<uint8_t S, uint8_t T, typename O>
struct js_name<bts::db::object_id<S,T,O> >
{
   static std::string name(){ return "st.protocol_id_type \"" + remove_namespace(fc::get_typename<O>::name()) + "\""; };
};

template<typename T>
struct js_name< fc::optional<T> >
{
   static std::string name(){ return "st.optional " + js_name<T>::name(); }
};

template<typename T>
struct js_name< fc::flat_set<T> >
{
   static std::string name(){ return "st.flat_set " + js_name<T>::name(); }
};

template<>
struct js_name< std::vector<char> >
{
   static std::string name(){ return "st.hex "; }
};
template<>
struct js_name< op_wrapper >
{
   static std::string name(){ return "st.operation "; }
};

template<typename T>
struct js_name< std::vector<T> >
{
   static std::string name(){ return "st.array " + js_name<T>::name(); }
};
template<typename T>
struct js_name< fc::safe<T> >
{
   static std::string name(){ return js_name<T>::name(); }
};

template<typename... T>
struct js_sv_name;

template<typename A>
struct js_sv_name<A>
{
   static std::string name(){ return "st." + js_name<A>::name() +" "; }
};

template<typename A, typename... T>
struct js_sv_name<A,T...>
{
   static std::string name(){ return "st." + js_name<A>::name() +", " + js_sv_name<T...>::name(); }
};


template<typename... T>
struct js_name< fc::static_variant<T...> >
{
   static std::string name(){ return "st.static_variant [" + js_sv_name<T...>::name() + "]"; }
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

struct serialize_visitor
{
   typedef void result_type;

   template<typename OperationType>
   result_type operator()( const OperationType& op )const
   {
      std::cerr << "register_op \"";
      std::cerr << remove_namespace( fc::get_typename<OperationType>::name() )  <<"\",\n";

      fc::reflector<OperationType>::visit( serialize_member_visitor() );

      std::cerr <<"\n";
   }
};

struct serialize_type_visitor
{
   typedef void result_type;

   int t = 0;
   serialize_type_visitor(int _t ):t(_t){}

   template<typename OperationType>
   result_type operator()( const OperationType& op )const
   {
      std::cerr << "    " <<remove_namespace( fc::get_typename<OperationType>::name() )  <<": "<<t<<"\n";
   }
};

int main( int argc, char** argv )
{
    operation op;

    std::cerr << "ChainTypes.operations=\n";
    for( uint32_t i = 0; i <= 37; ++i )
    {
       op.set_which(i);
       op.visit( serialize_type_visitor(i) );
    }
    std::cerr << "\n";

    for( uint32_t i = 0; i < 37; ++i )
    {
       op.set_which(i);
       op.visit( serialize_visitor() );
    }
    return 0;
}
