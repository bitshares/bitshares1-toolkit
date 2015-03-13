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
      asset operator -()const { return asset( -amount, asset_id ); }
      friend bool operator == ( const asset& a, const asset& b )
      {
         return tie(a.asset_id,a.amount) == tie(b.asset_id,b.amount);
      }
      friend bool operator >= ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return a.amount >= b.amount;
      }
      friend bool operator > ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return a.amount > b.amount;
      }
      friend asset operator - ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return asset( a.amount - b.amount, a.asset_id );
      }

   };


   struct price
   {
      asset base;
      asset quote;

   };
   inline price operator / ( const asset& base, const asset& quote )
   {
      return price{base,quote};
   }
   inline price operator~( const price& p ) { return price{p.quote,p.base}; }

   bool  operator <  ( const asset& a, const asset& b );
   bool  operator <= ( const asset& a, const asset& b );
   bool  operator <  ( const price& a, const price& b );
   bool  operator <= ( const price& a, const price& b );
   bool  operator >  ( const price& a, const price& b );
   bool  operator >= ( const price& a, const price& b );
   inline bool  operator == ( const price& a, const price& b ) { return std::tie(a.base,a.quote) == std::tie(b.base,b.quote); }
   asset operator *  ( const asset& a, const price& b );

   /**
    *  @class price_feed
    *  @brief defines market parameters for shorts and margin positions
    */
   struct price_feed
   {
      /**
       * A fair market exchange rate between debt and collateral
       */
      price rate;

      /**
       *  Required maitenance collateral is defined
       *  as a fixed point number with a maximum value of 10.000
       *  and a minimum value of 1.000.  
       *
       *  This value must be greater than required_maitenance_collateral or
       *  a margin call would be triggered immediately. 
       *
       *  Default requirement is $2 of collateral per $1 of debt based
       *  upon the premise that both parties to every trade should bring
       *  equal value to the table.
       */
      uint16_t required_initial_collateral = 2000;

      /**
       *  Required maitenance collateral is defined
       *  as a fixed point number with a maximum value of 10.000
       *  and a minimum value of 1.000.  
       *  
       *  A black swan event occurs when value_of_collateral equals
       *  value_of_debt, to avoid a black swan a margin call is
       *  executed when value_of_debt * required_maitenance_collateral 
       *  equals value_of_collateral using rate.
       *
       *  Default requirement is $1.75 of collateral per $1 of debt
       */
      uint16_t required_maitenance_collateral = 1750;

      friend bool operator < ( const price_feed& a, const price_feed& b )
      {
         return std::tie( a.rate, a.required_initial_collateral, a.required_maitenance_collateral ) <
                std::tie( b.rate, b.required_initial_collateral, b.required_maitenance_collateral );
      }

      friend bool operator == ( const price_feed& a, const price_feed& b )
      {
         return std::tie( a.rate, a.required_initial_collateral, a.required_maitenance_collateral ) == 
                std::tie( b.rate, b.required_initial_collateral, b.required_maitenance_collateral );
      }
   };

} }

FC_REFLECT( bts::chain::asset, (amount)(asset_id) )
FC_REFLECT( bts::chain::price, (base)(quote) )
FC_REFLECT( bts::chain::price_feed, (rate)(required_initial_collateral)(required_maitenance_collateral) )
