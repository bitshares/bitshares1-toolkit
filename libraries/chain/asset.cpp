#include <bts/chain/asset.hpp>
//#include <boost/rational.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {
      bool operator < ( const price& a, const price& b )
      {
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;

         FC_ASSERT( a.base.amount.value > 0 );
         FC_ASSERT( b.base.amount.value > 0 );
         auto amult = (fc::uint128(BTS_MAX_SHARE_SUPPLY) * a.quote.amount.value)/a.base.amount.value;
         auto bmult = (fc::uint128(BTS_MAX_SHARE_SUPPLY) * b.quote.amount.value)/b.base.amount.value;
         return amult < bmult;
         //return boost::rational<uint64_t>(a.quote.amount.value,a.base.amount.value) 
         //       <
         //       boost::rational<uint64_t>(b.quote.amount.value,b.base.amount.value);
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
