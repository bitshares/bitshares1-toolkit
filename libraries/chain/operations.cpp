#include <bts/chain/database.hpp>
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
   if( upgrade_to_prime ) return schedule.at(prime_upgrade_fee_type);
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
   FC_ASSERT( owner || active || voting_account || memo_key || vote );
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
   active_auth_set.insert(registrar);
}

void account_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( is_valid_name( name ) );
   FC_ASSERT( referrer_percent >= 0   );
   FC_ASSERT( referrer_percent <= 100 );
   auto pos = name.find( '/' );
   if( pos != string::npos )
   {
      FC_ASSERT( owner.weight_threshold == 1 );
      FC_ASSERT( owner.auths.size() == 1 );
   }
   FC_ASSERT( num_witness + num_committee >= num_witness );  // no overflow
   FC_ASSERT( num_witness + num_committee <= vote.size() );
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
   FC_ASSERT( force_settlement_offset_percent <= BTS_MAX_FORCE_SETTLEMENT_OFFSET );
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

asset_update_operation::asset_update_operation(const asset_object& old)
{
   issuer = old.issuer;
   asset_to_update = old.get_id();
   market_fee_percent = old.market_fee_percent;
   max_market_fee = old.max_market_fee;
   min_market_fee = old.min_market_fee;
   force_settlement_delay_sec = old.force_settlement_delay_sec;
   force_settlement_offset_percent = old.force_settlement_offset_percent;
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
   FC_ASSERT( force_settlement_offset_percent <= BTS_MAX_FORCE_SETTLEMENT_OFFSET );
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

void delegate_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(delegate_account);
}

void delegate_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
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
   if( amount_to_cover.amount == 0 )   FC_ASSERT( collateral_to_add.amount >= 0 );
   if( collateral_to_add.amount.value <= 0 ) FC_ASSERT( amount_to_cover.amount.value > 0 );

   FC_ASSERT( amount_to_cover.amount >= 0 );
   FC_ASSERT( amount_to_cover.asset_id != collateral_to_add.asset_id );
   FC_ASSERT( maintenance_collateral_ratio == 0 || maintenance_collateral_ratio >= 1000 );
}

share_type call_order_update_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( short_order_fee_type );
}

proposal_create_operation proposal_create_operation::genesis_proposal(const database& db)
{
   auto global_params = db.get_global_properties().parameters;
   proposal_create_operation op = {account_id_type(), asset(), {},
                                   db.head_block_time() + global_params.maximum_proposal_lifetime,
                                   global_params.genesis_proposal_review_period};
   op.fee = op.calculate_fee(global_params.current_fees);
   return op;
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
               owner_approvals_to_add.empty() && owner_approvals_to_remove.empty() &&
               key_approvals_to_add.empty() && key_approvals_to_remove.empty()));
   for( auto a : active_approvals_to_add )
      FC_ASSERT(active_approvals_to_remove.find(a) == active_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
   for( auto a : owner_approvals_to_add )
      FC_ASSERT(owner_approvals_to_remove.find(a) == owner_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
   for( auto a : key_approvals_to_add )
      FC_ASSERT(key_approvals_to_remove.find(a) == key_approvals_to_remove.end(),
                "Cannot add and remove approval at the same time.");
}

void proposal_delete_operation::get_required_auth(flat_set<account_id_type>& active_auth_set,
                                                  flat_set<account_id_type>& owner_auth_set) const
{
   if( using_owner_authority )
      owner_auth_set.insert(fee_paying_account);
   else
      active_auth_set.insert(fee_paying_account);
}

void account_transfer_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

void account_transfer_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( account_id );
}

share_type  account_transfer_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at(transfer_fee_type);
}


void account_claim_cashback_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount >= 0 );
}

void account_claim_cashback_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( account );
}

share_type  account_claim_cashback_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at(transfer_fee_type);
}

void proposal_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
}

void witness_withdraw_pay_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(to_account);
}

void witness_withdraw_pay_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount >= 0 );
}

share_type witness_withdraw_pay_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(witness_withdraw_pay_fee_type);
}

void account_whitelist_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(authorizing_account);
}

void global_parameters_update_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   new_parameters.validate();
}

share_type global_parameters_update_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(global_parameters_update_fee_type);
}

void witness_create_operation::get_required_auth(flat_set<bts::chain::account_id_type>& active_auth_set, flat_set<bts::chain::account_id_type>&) const
{
   active_auth_set.insert(witness_account);
}

void witness_create_operation::validate() const
{
   FC_ASSERT(fee.amount >= 0);
}

share_type witness_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(delegate_create_fee_type);
}

void withdraw_permission_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( withdraw_from_account );
}

void withdraw_permission_update_operation::validate()const
{
   FC_ASSERT( withdrawal_limit.amount > 0 );
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdrawal_period_sec > 0 );
}

share_type withdraw_permission_update_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   return schedule.at( withdraw_permission_update_fee_type );
}

void withdraw_permission_claim_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( withdraw_to_account );
}

void withdraw_permission_claim_operation::validate()const
{
   FC_ASSERT( withdraw_to_account != withdraw_from_account );
   FC_ASSERT( amount_to_withdraw.amount > 0 );
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdraw_permission.instance.value != 0 );
}

share_type withdraw_permission_claim_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( transfer_fee_type );
   bts_fee_required += share_type((memo.size() * schedule.at( data_fee_type ).value)/1024);
   return bts_fee_required;
}

void withdraw_permission_delete_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(withdraw_from_account);
}

void withdraw_permission_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type withdraw_permission_delete_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(withdraw_permission_update_fee_type);
}

void withdraw_permission_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(withdraw_from_account);
}

void withdraw_permission_create_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdrawal_limit.amount > 0 );
   //TODO: better bounds checking on these values
   FC_ASSERT( withdrawal_period_sec > 0 );
   FC_ASSERT( periods_until_expiration > 0 );
}

share_type withdraw_permission_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(withdraw_permission_update_fee_type);
}

void asset_settle_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert( account );
}

void asset_settle_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount.amount >= 0 );
}

share_type asset_settle_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(asset_settle_fee_type);
}

void create_bond_offer_operation::get_required_auth( flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& )const
{
   active_auth_set.insert( creator );
}

void create_bond_offer_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount.amount > 0 );
   FC_ASSERT( amount.asset_id == collateral_rate.base.asset_id || amount.asset_id == collateral_rate.quote.asset_id );
   collateral_rate.validate();
   FC_ASSERT( min_loan_period_sec > 0 );
   FC_ASSERT( loan_period_sec >= min_loan_period_sec );
   FC_ASSERT( interest_apr <= MAX_INTEREST_APR );
}

share_type create_bond_offer_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   return schedule.at( create_bond_offer_fee_type );
}

} } // namespace bts::chain
