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
         auto amult = fc::uint128(a.quote.amount.value) * b.base.amount.value;
         auto bmult = fc::uint128(b.quote.amount.value) * a.base.amount.value;
         return amult > bmult;
      }
      bool operator <= ( const price& a, const price& b )
      {
         //wdump((a)(b));
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;
         auto amult = fc::uint128(a.quote.amount.value) * b.base.amount.value;
         auto bmult = fc::uint128(b.quote.amount.value) * a.base.amount.value;
         //wdump( (a.to_real())(b.to_real()));
         //wdump( (amult)(bmult) );
         return amult >= bmult;
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

      bool operator >= ( const price& a, const price& b )
      {
         return !(a < b);
      }
      bool operator > ( const price& a, const price& b )
      {
         return a <= b;
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
} } // bts::chain
