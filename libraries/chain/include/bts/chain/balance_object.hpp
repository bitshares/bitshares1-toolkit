#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/asset.hpp>

namespace bts { namespace chain { 
   class account_object;

   class balance_object : public object
   {
      public:
         static const object_type type = balance_object_type;

         balance_object():object( balance_object_type ){}

         balance_object( const address_authority& o, const asset& b )
         :object(balance_object_type),owner(o),balance(b){}

         address_authority  owner;
         asset              balance;
   };
} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::balance_object, (bts::chain::object), (owner)(balance) )
