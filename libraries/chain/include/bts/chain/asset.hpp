#pragma once
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   struct asset 
   {
      asset( share_type a = 0, asset_id_type id = asset_id_type() )
      :amount(a),asset_id(id){}

      share_type    amount;
      asset_id_type asset_id;

      asset& operator += ( const asset& o )
      {
         FC_ASSERT( asset_id == o.asset_id );
         amount += o.amount;
         return *this;
      }
      asset& operator -= ( const asset& o )
      {
         FC_ASSERT( asset_id == o.asset_id );
         amount -= o.amount;
         return *this;
      }
      friend bool operator == ( const asset& a, const asset& b )
      {
         return tie(a.asset_id,a.amount) == tie(b.asset_id,b.amount);
      }
      friend bool operator >= ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return a.amount >= b.amount;
      }
   };

   struct price 
   {
      asset base;
      asset quote;
   };
   bool  operator < ( const price& a, const price& b );
   asset operator * ( const asset& a, const price& b );

} }

FC_REFLECT( bts::chain::asset, (amount)(asset_id) )
FC_REFLECT( bts::chain::price, (base)(quote) )
