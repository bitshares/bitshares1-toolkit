#pragma once
#include <bts/chain/evaluator.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

   class account_create_evaluator : public evaluator<account_create_operation>
   {
      public:
         static bool is_valid_name( const string& s );
         static bool is_premium_name( const string& n );
         static bool is_cheap_name( const string& n );
         
         virtual object_id_type evaluate( const operation& o ) override;
         virtual object_id_type apply( const operation& o ) override;
   };

} } // bts::chain
