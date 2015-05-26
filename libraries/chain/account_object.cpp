#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {

bool account_object::is_authorized_asset(const asset_object& asset_obj) const {
   for( const auto id : blacklisting_accounts )
      if( asset_obj.options.blacklist_authorities.find(id) != asset_obj.options.blacklist_authorities.end() ) return false;

   for( const auto id : whitelisting_accounts )
      if( asset_obj.options.whitelist_authorities.find(id) != asset_obj.options.whitelist_authorities.end() ) return true;
   return false;
}

void account_balance_object::adjust_balance(const asset& delta)
{
   assert(delta.asset_id == asset_type);
   share_type new_balance(balance);
   new_balance += delta.amount;
   balance = new_balance.value;
}

} } // bts::chain
