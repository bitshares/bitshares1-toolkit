#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <fc/crypto/aes.hpp>

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
   if( s.size() <  2  ) return false;
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

bool is_cheap_name( const string& n )
{
   bool v = false;
   for( auto c : n )
   {
      if( c >= '0' && c <= '9' ) return true;
      if( c == '.' || c == '-' || c == '/' ) return true;
      switch( c )
      {
         case 'a':
         case 'e':
         case 'i':
         case 'o':
         case 'u':
         case 'y':
            v = true;
      }
   }
   if( !v )
      return true;
   return false;
}

share_type account_create_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at(account_create_fee_type);

   uint32_t s = name.size();
   if( is_cheap_name( name ) ) s = 63;

   FC_ASSERT( s >= 2 );

   if( s <= 8 )
     bts_fee_required = schedule.at(account_create_fee_type+9-s);

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
   FC_ASSERT( owner || active || voting_account || memo_key || vote || upgrade_to_prime );
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
   if( memo )
   {
      bts_fee_required += share_type((memo->message.size() * schedule.at( data_fee_type ).value)/1024);
   }
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
   // FC_ASSERT( (num_witness == 0) || (num_witness&0x01) == 0, "must be odd number" );    
   // FC_ASSERT( (num_committee == 0) || (num_committee&0x01) == 0, "must be odd number" ); 
}


share_type asset_publish_feed_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   return schedule.at( publish_feed_fee_type );
}

void asset_publish_feed_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   feed.validate();
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
   common_options.validate();
   if( common_options.flags & (disable_force_settle|global_settle) )
      FC_ASSERT( common_options.flags & market_issued );
   FC_ASSERT( bitasset_options.valid() == bool(common_options.flags & market_issued) );
   if( bitasset_options ) bitasset_options->validate();

   asset dummy = asset(1) * common_options.core_exchange_rate;
   FC_ASSERT(dummy.asset_id == asset_id_type(1));
   FC_ASSERT(precision <= 12);
}

asset_update_operation::asset_update_operation(const asset_object& old)
{
   issuer = old.issuer;
   asset_to_update = old.get_id();
   new_options = old.options;
}

void asset_update_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(issuer);
}

void asset_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   if( new_issuer )
      FC_ASSERT(issuer != *new_issuer);
   new_options.validate();

   asset dummy = asset(1, asset_to_update) * new_options.core_exchange_rate;
   FC_ASSERT(dummy.asset_id == asset_id_type());
}

share_type asset_update_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( asset_update_fee_type );
}

void asset_burn_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(payer);
}

void asset_burn_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount_to_burn.amount.value <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( amount_to_burn.amount.value > 0 );
}

share_type asset_burn_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( asset_issue_fee_type );
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
   FC_ASSERT( withdraw_from_account != authorized_account );
   FC_ASSERT( periods_until_expiration > 0 );
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
}

share_type withdraw_permission_claim_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( transfer_fee_type );
   if( memo )
      bts_fee_required += share_type((memo->message.size() * schedule.at( data_fee_type ).value)/1024);
   return bts_fee_required;
}

void withdraw_permission_delete_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(withdraw_from_account);
}

void withdraw_permission_delete_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( withdraw_from_account != authorized_account );
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
   FC_ASSERT( withdraw_from_account != authorized_account );
   FC_ASSERT( withdrawal_limit.amount > 0 );
   //TODO: better bounds checking on these values
   FC_ASSERT( withdrawal_period_sec > 0 );
   FC_ASSERT( periods_until_expiration > 0 );
}

share_type withdraw_permission_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at(withdraw_permission_update_fee_type);
}


void        asset_global_settle_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( fee_payer() );

}

void        asset_global_settle_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( asset_to_settle == settle_price.base.asset_id );
}

share_type  asset_global_settle_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at(global_settle_fee_type);
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

void bond_create_offer_operation::get_required_auth( flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>& )const
{
   active_auth_set.insert( creator );
}

void bond_create_offer_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount.amount > 0 );
   FC_ASSERT( amount.asset_id == collateral_rate.base.asset_id || amount.asset_id == collateral_rate.quote.asset_id );
   collateral_rate.validate();
   FC_ASSERT( min_loan_period_sec > 0 );
   FC_ASSERT( loan_period_sec >= min_loan_period_sec );
   FC_ASSERT( interest_apr <= MAX_INTEREST_APR );
}

share_type bond_create_offer_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   return schedule.at( create_bond_offer_fee_type );
}

void bts::chain::asset_publish_feed_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(publisher);
}

void asset_update_bitasset_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&) const
{
   active_auth_set.insert(issuer);
}

void asset_update_bitasset_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
   new_options.validate();
}

share_type asset_update_bitasset_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( asset_update_fee_type );
}

void asset_update_feed_producers_operation::validate() const
{
   FC_ASSERT( fee.amount >= 0 );
}
void            file_write_operation::validate()const
{
   FC_ASSERT( uint32_t(offset) + data.size() <= file_size );
   FC_ASSERT( flags <= 0x2f );
   FC_ASSERT( file_size > 0 );
   /** less than 10 years to prevent overflow of 64 bit numbers in the value*lease_seconds*file_size calculation */
   FC_ASSERT( lease_seconds < 60*60*24*365*10 );
}

share_type      file_write_operation::calculate_fee( const fee_schedule_type& k )const
{
   return ((((k.at( file_storage_fee_per_day_type ).value * lease_seconds)/(60*60*24))*file_size)/0xff) + ((data.size() * k.at( data_fee_type ).value)/1024);
}


void vesting_balance_create_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   // owner's authorization isn't needed since this is effectively a transfer of value TO the owner
   active_auth_set.insert( creator );
}

share_type vesting_balance_create_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( vesting_balance_create_fee_type );
}

void vesting_balance_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount.amount > 0 );
   FC_ASSERT( vesting_seconds > 0 );
}

void vesting_balance_withdraw_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   active_auth_set.insert( owner );
}

void vesting_balance_withdraw_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount.amount > 0 );
}

share_type vesting_balance_withdraw_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( vesting_balance_withdraw_fee_type );
}

void         memo_data::set_message( const fc::ecc::private_key& priv,
                                     const fc::ecc::public_key& pub, const string& msg )
{
   if( from )
   {
      auto secret = priv.get_shared_secret(pub);
      digest_type::encoder enc;
      fc::raw::pack(enc, secret);
      fc::raw::pack(enc, msg);
      memo_message memo(enc.result()._hash[0], msg);
      message = fc::aes_encrypt( secret, fc::raw::pack( memo ) );
   }
   else
   {
      message = fc::raw::pack( memo_message( 0, msg ) );
   }
}

string memo_data::get_message( const fc::ecc::private_key& priv,
                               const fc::ecc::public_key& pub )const
{
   if( from )
   {
      auto secret = priv.get_shared_secret(pub);
      auto plain_text = fc::aes_decrypt( secret, message );
      auto result = fc::raw::unpack<memo_message>(plain_text);
      digest_type::encoder enc;
      fc::raw::pack(enc, secret);
      fc::raw::pack(enc, result.text);
      FC_ASSERT( result.checksum == uint32_t(enc.result()._hash[0]) );
      return result.text;
   }
   else
   {
      return fc::raw::unpack<memo_message>(message).text;
   }
}

void        custom_operation::get_required_auth(flat_set<account_id_type>& active_auth_set, flat_set<account_id_type>&)const
{
   for( auto item : required_auths )
      active_auth_set.insert(item);
}
void        custom_operation::validate()const
{
   FC_ASSERT( fee.amount > 0 );
}
share_type  custom_operation::calculate_fee( const fee_schedule_type& k )const
{
   return (data.size() * k.at( data_fee_type ).value)/1024;
}

} } // namespace bts::chain
