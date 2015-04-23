#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace chain {

void account_statistics_object::adjust_cashback( share_type amount, time_point_sec maturity, time_point_sec current_time )
{ try {
   if( amount == 0 ) return;

   fc::uint128      current_maturity((current_time - cashback_maturity).to_seconds());
   fc::uint128      add_maturity((current_time - maturity).to_seconds());

   auto new_maturity = (current_maturity * cashback_rewards.value) + (add_maturity * amount.value);
   cashback_rewards += amount;
   new_maturity /= cashback_rewards.value;

   cashback_maturity = fc::time_point() + fc::seconds( new_maturity.to_uint64() );
} FC_CAPTURE_AND_RETHROW( (amount)(maturity)(current_time) ) }

bool account_object::is_authorized_asset(const asset_object& asset_obj) const {
   for( const auto id : blacklisting_accounts )
      if( asset_obj.blacklist_authorities.find(id) != asset_obj.blacklist_authorities.end() ) return false;

   for( const auto id : whitelisting_accounts )
      if( asset_obj.whitelist_authorities.find(id) != asset_obj.whitelist_authorities.end() ) return true;
   return false;
}

void account_balance_object::adjust_balance(const asset& delta)
{
   assert(delta.asset_id == asset_type);
   balance += delta.amount;
}

} } // bts::chain
