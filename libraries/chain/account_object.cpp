#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>

namespace bts { namespace chain {

void account_balance_object::add_balance( const asset& a )
{
   FC_ASSERT( a.amount > 0 );
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(a.asset_id,share_type(0)) );
   if( lb != vec.end() && lb->first == a.asset_id ) lb->second += a.amount;
   else
   {
      vec.insert( lb, std::make_pair( a.asset_id, a.amount ) );
   }
}
asset account_balance_object::get_balance( asset_id_type what )const
{
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(what,share_type(0)) );
   if( lb != vec.end() && lb->first == what ) return asset{lb->second,what};
   return asset{0,what};
}

void account_balance_object::sub_balance( const asset& a )
{
   FC_ASSERT( a.amount > 0 );
   auto& vec = balances;
   auto lb = std::lower_bound( vec.begin(), vec.end(), std::make_pair(a.asset_id,share_type(0)) );
   if( lb != vec.end() && lb->first == a.asset_id ) lb->second -= a.amount;
   else
   {
      FC_ASSERT( false, "No current Balance for Asset" );
   }
}

bool account_object::is_authorized_asset(const asset_object& asset_obj) const {
   for( const auto id : blacklisting_accounts )
      if( asset_obj.blacklist_authorities.find(id) != asset_obj.blacklist_authorities.end() ) return false;

   for( const auto id : whitelisting_accounts )
      if( asset_obj.whitelist_authorities.find(id) != asset_obj.whitelist_authorities.end() ) return true;
   return false;
}

} } // bts::chain
