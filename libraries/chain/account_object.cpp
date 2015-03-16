#include <bts/chain/account_object.hpp>
namespace bts { namespace chain {
void account_object::authorize_asset(asset_id_type asset_id, bool state)
{
   if( state )
      FC_ASSERT(authorized_assets.insert_unique(asset_id).second);
   else
      authorized_assets.erase(asset_id);
}

bool  account_object::is_authorized_asset( asset_id_type asset_id )const
{
   return std::binary_search( authorized_assets.begin(), authorized_assets.end(), asset_id );
}

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

} } // bts::chain
