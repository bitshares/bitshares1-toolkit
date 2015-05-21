#include <bts/chain/asset.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
      bool operator < ( const asset& a, const asset& b )
      {
         return std::tie( a.asset_id, a.amount ) < std::tie( b.asset_id, b.amount);
      }
      bool operator <= ( const asset& a, const asset& b )
      {
         return std::tie( a.asset_id, a.amount ) <= std::tie( b.asset_id, b.amount);
      }
      bool operator < ( const price& a, const price& b )
      {
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;
         auto amult = fc::uint128(b.quote.amount.value) * a.base.amount.value;
         auto bmult = fc::uint128(a.quote.amount.value) * b.base.amount.value;
         assert( (a.to_real() < b.to_real()) == (amult < bmult) );
         return amult < bmult;
      }
      bool operator <= ( const price& a, const price& b )
      {
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;
         auto amult = fc::uint128(b.quote.amount.value) * a.base.amount.value;
         auto bmult = fc::uint128(a.quote.amount.value) * b.base.amount.value;
         assert( (a.to_real() <= b.to_real()) == (amult <= bmult) );
         return amult <= bmult;
      }
      bool operator == ( const price& a, const price& b )
      {
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;
         auto amult = fc::uint128(a.quote.amount.value) * b.base.amount.value;
         auto bmult = fc::uint128(b.quote.amount.value) * a.base.amount.value;
         return amult == bmult;
      }
      bool operator != ( const price& a, const price& b )
      {
         return !(a==b);
      }

      bool operator >= ( const price& a, const price& b )
      {
         return !(a < b);
      }
      bool operator > ( const price& a, const price& b )
      {
         return !(a <= b);
      }

      asset operator * ( const asset& a, const price& b )
      {
         if( a.asset_id == b.base.asset_id )
         {
            FC_ASSERT( b.base.amount.value > 0 );
            auto result = (fc::uint128(a.amount.value) * b.quote.amount.value)/b.base.amount.value;
            FC_ASSERT( result <= BTS_MAX_SHARE_SUPPLY );
            return asset( result.to_uint64(), b.quote.asset_id );
         }
         else if( a.asset_id == b.quote.asset_id )
         {
            FC_ASSERT( b.quote.amount.value > 0 );
            auto result = (fc::uint128(a.amount.value) * b.base.amount.value)/b.quote.amount.value;
            FC_ASSERT( result <= BTS_MAX_SHARE_SUPPLY );
            return asset( result.to_uint64(), b.base.asset_id );
         }
         FC_ASSERT( !"invalid asset * price", "", ("asset",a)("price",b) );
      }

      price operator / ( const asset& base, const asset& quote )
      {
         FC_ASSERT( base.asset_id != quote.asset_id );
         return price{base,quote};
      }
      price price::max( asset_id_type base, asset_id_type quote ) { return asset( share_type(BTS_MAX_SHARE_SUPPLY), base ) / asset( share_type(1), quote); }
      price price::min( asset_id_type base, asset_id_type quote ) { return asset( 1, base ) / asset( BTS_MAX_SHARE_SUPPLY, quote); }

      price price::call_price(const asset& debt, const asset& collateral, uint16_t collateral_ratio)
      {
         fc::uint128 tmp( collateral.amount.value );
         tmp *= collateral_ratio - 1000;
         tmp /= 1000;
         FC_ASSERT( tmp <= BTS_MAX_SHARE_SUPPLY );
         return asset( tmp.to_uint64(), collateral.asset_id) / debt;
      }

      bool price::is_null() const { return *this == price(); }

      void price::validate() const
      { try {
         FC_ASSERT( base.amount > share_type(0) );
         FC_ASSERT( quote.amount > share_type(0) );
         FC_ASSERT( base.asset_id != quote.asset_id );
      } FC_CAPTURE_AND_RETHROW() }

      void price_feed::validate() const
      { try {
         if( !call_limit.is_null() )
            call_limit.validate();
         if( !short_limit.is_null() )
            short_limit.validate();
         if( !settlement_price.is_null() )
            settlement_price.validate();
         FC_ASSERT( call_limit.is_null() == short_limit.is_null() );
         FC_ASSERT( call_limit.base.asset_id == short_limit.quote.asset_id );
         FC_ASSERT( call_limit.quote.asset_id == short_limit.base.asset_id );
         FC_ASSERT( max_margin_period_sec > 0 );
         FC_ASSERT( required_maintenance_collateral < required_initial_collateral );
         FC_ASSERT( required_maintenance_collateral >= 1000 );
         FC_ASSERT( call_limit.is_null() || call_limit < ~short_limit );
      } FC_CAPTURE_AND_RETHROW() }

} } // bts::chain
