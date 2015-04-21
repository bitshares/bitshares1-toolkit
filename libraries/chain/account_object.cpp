#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <fc/uint128.hpp>

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

share_type account_balance_object::voting_weight() const
{
   return get_balance(asset_id_type()).amount + total_core_in_orders;
}

void account_balance_object::adjust_cashback( share_type amount, time_point_sec maturity, time_point_sec current_time )
{
   fc::uint128      current_maturity((current_time - cashback_maturity).to_seconds());
   fc::uint128      add_maturity((current_time - maturity).to_seconds());

   wdump((cashback_rewards));
   wdump((current_maturity)(add_maturity) );
   auto new_maturity = (current_maturity * cashback_rewards.value) + (add_maturity * amount.value);
   cashback_rewards += amount;
   FC_ASSERT( cashback_rewards.value > 0 );
   new_maturity /= cashback_rewards.value;
   wdump((cashback_rewards));

   cashback_maturity = fc::time_point() + fc::seconds( new_maturity.to_uint64() );
   wdump((cashback_maturity));
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
