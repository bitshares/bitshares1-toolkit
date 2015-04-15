#include <bts/chain/operations.hpp>

namespace bts { namespace chain {

/**
 *  Valid symbols have between 3 and 17 upper case characters
 *  with at most a single "." that is not the first or last character.
 */
bool is_valid_symbol( const string& symbol )
{
   if( symbol.size() > 17 ) return false;
   if( symbol.size() < 3  ) return false;
   int dot_count = 0;
   for( auto c : symbol )
   {
      if( c == '.' ) ++dot_count;
      else if( c < 'A' || c > 'Z' ) return false;
   }
   if( symbol[0] == '.' || symbol[symbol.size()-1] == '.' )
      return false;
   return dot_count <= 1;
}

/**
 *  Valid names are all lower case, start with [a-z] and may
 *  have "." or "-" in the name along with a single '/'.  The
 *  next character after a "/", "." or "-" cannot be [0-9] or
 *  another '.', '-'.
 *
 */
bool is_valid_name( const string& s )
{
   if( s.size() == 0  ) return true;
   if( s.size() <  3  ) return false;
   if( s.size() >= 64 ) return false;

   int num_slash = 0;
   char prev = ' ';
   for( auto c : s )
   {
      if( c >= 'a' && c <= 'z' ){}
      else if( c >= '0' && c <= '9' )
      {
         if( prev == ' ' || prev == '.' || prev == '-' || prev == '/' ) return false;
      }
      else switch( c )
      {
            case '/':
               if( ++num_slash > 1 ) return false;
            case '.':
            case '-':
               if( prev == ' ' || prev == '/' || prev == '.' || prev == '-' ) return false;
              break;
            default:
              return false;
      }
      prev = c;
   }
   switch( s.back() )
   {
      case '/': case '-': case '.':
         return false;
      default:
         return true;
   }
}
bool is_premium_name( const string& n )
{
   return false;
}
bool is_cheap_name( const string& n )
{
   for( auto c : n )
   {
      if( c >= '0' && c <= '9' ) return true;
      if( c == '.' || c == '-' || c == '/' ) return true;
   }
   return false;
}

share_type account_create_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at(account_create_fee_type);

   if( name.size() )
   {
      uint32_t s = name.size();
      if( is_premium_name( name ) )    s = 2;
      else if( is_cheap_name( name ) ) s = 63;
      while( s <= 8 ) {  bts_fee_required *= 10; ++s; }

   }
   return bts_fee_required;
}
share_type account_update_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   return schedule.at(account_create_fee_type);
}
void account_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                                 flat_set<account_id_type>& owner_auth_set) const
{
   if( owner || active )
      owner_auth_set.insert( account );
   else
      active_auth_set.insert( account );
}

void account_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( account != account_id_type() );
   FC_ASSERT( owner || active || voting_key || memo_key || vote );
}


share_type asset_create_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at(asset_create_fee_type);

   uint32_t s = symbol.size();
   while( s <= 6 ) {  bts_fee_required *= 30; ++s; }

   return bts_fee_required;
}

share_type transfer_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( transfer_fee_type );
   bts_fee_required += share_type((memo.size() * schedule.at( data_fee_type ).value)/1024);
   return bts_fee_required;
}

struct key_data_validate
{
   typedef void result_type;
   void operator()( const address& a )const { FC_ASSERT( a != address() ); }
   void operator()( const public_key_type& a )const { FC_ASSERT( a != public_key_type() ); }
};
void key_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                             flat_set<account_id_type>&) const
{
   active_auth_set.insert(fee_paying_account);
}

void key_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   key_data.visit( key_data_validate() );
}

void account_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                                 flat_set<account_id_type>&) const
{
   active_auth_set.insert(fee_paying_account);
}

void account_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( is_valid_name( name ) );
   auto pos = name.find( '/' );
   if( pos != string::npos )
   {
      FC_ASSERT( owner.weight_threshold == 1 );
      FC_ASSERT( owner.auths.size() == 1 );
   }
}


share_type delegate_publish_feeds_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( publish_feed_fee_type );
   bts_fee_required *= feeds.size();
   return bts_fee_required;
}

void delegate_publish_feeds_operation::validate()const
{
   FC_ASSERT( feeds.size() > 0 );
   FC_ASSERT( fee.amount >= 0 );
   optional<price_feed> prev;
   for( const price_feed& item : feeds )
   {
      item.validate();
      if( prev )
         //Verify uniqueness and sortedness.
         FC_ASSERT( std::tie(prev->call_limit.base.asset_id, prev->call_limit.quote.asset_id) <
                    std::tie(item.call_limit.base.asset_id, item.call_limit.quote.asset_id) );
      prev = item;
   }
}

void transfer_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                           flat_set<account_id_type>&) const
{
   active_auth_set.insert( from );
}

void transfer_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( from != to );
   FC_ASSERT( amount.amount > 0 );
}

void asset_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(issuer);
}

void  asset_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( is_valid_symbol( symbol ) );
   FC_ASSERT( max_supply <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( max_supply > 0 );
   FC_ASSERT( market_fee_percent <= BTS_MAX_MARKET_FEE_PERCENT );
   FC_ASSERT( permissions <= ASSET_ISSUER_PERMISSION_MASK );
   FC_ASSERT( flags <= ASSET_ISSUER_PERMISSION_MASK );
   FC_ASSERT( core_exchange_rate.quote.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.amount > 0 );
   FC_ASSERT( core_exchange_rate.quote.amount > 0 );

   FC_ASSERT( !(flags & ~permissions ) );
   if( permissions & market_issued )
   {
      FC_ASSERT( (permissions == market_issued) );
      FC_ASSERT( (flags == market_issued) );
   }
}

void asset_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(issuer);
}

void asset_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( new_issuer || permissions || flags || core_exchange_rate || new_price_feed || new_whitelist_authorities
              || new_blacklist_authorities );

   if( permissions )
   {
      if( flags )
         FC_ASSERT( !(*flags & ~*permissions ) );
      FC_ASSERT( *permissions <= ASSET_ISSUER_PERMISSION_MASK );
   }
   if( flags )
      FC_ASSERT( *flags <= ASSET_ISSUER_PERMISSION_MASK );

   FC_ASSERT( !(core_exchange_rate.valid() && new_price_feed.valid()) );

   if( core_exchange_rate )
   {
      core_exchange_rate->validate();
      FC_ASSERT(core_exchange_rate->quote.asset_id == asset_to_update);
      FC_ASSERT(core_exchange_rate->base.asset_id == asset_id_type());
   }
   if( new_price_feed )
   {
      new_price_feed->validate();
      if( !new_price_feed->call_limit.is_null() )
      {
         FC_ASSERT(new_price_feed->call_limit.quote.asset_id == asset_to_update);
         FC_ASSERT(new_price_feed->call_limit.base.asset_id < asset_to_update);
      }
   }
}

share_type asset_update_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( asset_update_fee_type );
}

void asset_issue_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(issuer);
}

void asset_issue_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( asset_to_issue.amount.value <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( asset_to_issue.amount.value > 0 );
   FC_ASSERT( asset_to_issue.asset_id != 0 );
}

share_type asset_issue_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( asset_issue_fee_type );
}

share_type delegate_create_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( delegate_create_fee_type ) ;
}
share_type delegate_update_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( delegate_update_fee_type ) ;
}

void delegate_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(delegate_account);
}

void delegate_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( pay_rate <= 100 );
   FC_ASSERT( max_block_size >= BTS_MIN_BLOCK_SIZE_LIMIT );
   FC_ASSERT( max_transaction_size >= BTS_MIN_TRANSACTION_SIZE_LIMIT );
   FC_ASSERT( block_interval_sec > 0 && block_interval_sec <= BTS_MAX_BLOCK_INTERVAL );
   FC_ASSERT( max_sec_until_expiration > block_interval_sec );
   for( auto fe : fee_schedule.fees ) FC_ASSERT( fe > 0 );
}
void delegate_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(delegate_account);
}

void delegate_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( pay_rate <= 100 || pay_rate == 255 );
   FC_ASSERT( max_block_size >= BTS_MIN_BLOCK_SIZE_LIMIT );
   FC_ASSERT( max_transaction_size >= BTS_MIN_TRANSACTION_SIZE_LIMIT );
   FC_ASSERT( block_interval_sec > 0 && block_interval_sec <= BTS_MAX_BLOCK_INTERVAL );
   FC_ASSERT( max_sec_until_expiration > block_interval_sec );
   if( fee_schedule ) for( auto fe : fee_schedule->fees ) FC_ASSERT( fe > 0 );
}

void asset_fund_fee_pool_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(from_account);
}

void asset_fund_fee_pool_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( fee.asset_id == asset_id_type() );
   FC_ASSERT( amount > 0 );
}

share_type asset_fund_fee_pool_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( asset_fund_fee_pool_fee_type );
}

void limit_order_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(seller);
}

void limit_order_create_operation::validate()const
{
   FC_ASSERT( amount_to_sell.asset_id != min_to_receive.asset_id );
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount_to_sell.amount > 0 );
   FC_ASSERT( min_to_receive.amount > 0 );
}

share_type limit_order_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( limit_order_fee_type );
}

void limit_order_cancel_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(fee_paying_account);
}

void limit_order_cancel_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type limit_order_cancel_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( limit_order_fee_type );
}

void short_order_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(seller);
}

void short_order_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( initial_collateral_ratio >= BTS_MIN_COLLATERAL_RATIO     );
   FC_ASSERT( initial_collateral_ratio >  maintenance_collateral_ratio );
   FC_ASSERT( initial_collateral_ratio <= BTS_MAX_COLLATERAL_RATIO     );
}

share_type short_order_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( short_order_fee_type );
}
void short_order_cancel_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(fee_paying_account);
}

void short_order_cancel_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type short_order_cancel_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( short_order_fee_type );
}

void call_order_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(funding_account);
}

void call_order_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( collateral_to_add.amount > 0 || amount_to_cover.amount > 0 || maintenance_collateral_ratio > 0 );
   FC_ASSERT( collateral_to_add.amount >= 0 );
   FC_ASSERT( amount_to_cover.amount >= 0 );
   FC_ASSERT( amount_to_cover.asset_id != collateral_to_add.asset_id );
   FC_ASSERT( maintenance_collateral_ratio == 0 || maintenance_collateral_ratio >= 1000 );
}

share_type call_order_update_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( short_order_fee_type );
}

void proposal_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(fee_paying_account);
}

void proposal_create_operation::validate() const
{
   FC_ASSERT( !proposed_ops.empty() );
   for( const auto& op : proposed_ops ) op.validate();
}

void proposal_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                                  flat_set<account_id_type>& owner_auth_set) const
{
   active_auth_set.insert(fee_paying_account);
   for( auto id : active_approvals_to_add )
      active_auth_set.insert(id);
   for( auto id : active_approvals_to_remove )
      active_auth_set.insert(id);
   for( auto id : owner_approvals_to_add )
      owner_auth_set.insert(id);
   for( auto id : owner_approvals_to_remove )
      owner_auth_set.insert(id);
}

void proposal_update_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
   FC_ASSERT(!(active_approvals_to_add.empty() && active_approvals_to_remove.empty() &&
               owner_approvals_to_add.empty() && owner_approvals_to_remove.empty()));
}

void proposal_delete_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                                  flat_set<account_id_type>& owner_auth_set) const
{
   if( using_owner_authority )
      owner_auth_set.insert(fee_paying_account);
   else
      active_auth_set.insert(fee_paying_account);
}

void proposal_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
}

void delegate_withdraw_pay_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& owner_auth_set) const
{
   active_auth_set.insert(to_account);
}

void delegate_withdraw_pay_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount >= 0 );
}

share_type delegate_withdraw_pay_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(delegate_withdraw_pay_fee_type);
}

void account_whitelist_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(authorizing_account);
}

} } // namespace bts::chain
