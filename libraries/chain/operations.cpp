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
void account_update_operation::validate()const
{
   FC_ASSERT( fee.amount > 0 );
   FC_ASSERT( owner || active || voting_key || memo_key || vote );

   if( vote && vote->size() > 1 )
   {
      for( int i = 1; i < vote->size(); ++i )
         FC_ASSERT( vote->at(i-1) < vote->at(i) );
   }
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
void key_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   key_data.visit( key_data_validate() );
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


share_type account_publish_feeds_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( publish_feed_fee_type );
   return bts_fee_required;
}

void account_publish_feeds_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   optional<price> prev;
   for( auto item : feeds )
   {
      FC_ASSERT( item.base.amount >= share_type(0) ); // prevent divide by 0
      FC_ASSERT( item.quote.amount >= share_type(0) ); // prevent divide by 0
      if( prev )
      {
         FC_ASSERT( !(prev->base.asset_id == item.base.asset_id && prev->quote.asset_id == item.quote.asset_id) );
      }
      else prev = item;
   }
}

void transfer_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( from != to );
   FC_ASSERT( amount.amount > 0 );
}

void  asset_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( is_valid_symbol( symbol ) );
   FC_ASSERT( max_supply <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( max_supply > 0 );
   FC_ASSERT( market_fee_percent <= BTS_MAX_MARKET_FEE_PERCENT );
   FC_ASSERT( permissions <= market_issued );
   FC_ASSERT( flags <= market_issued );
   FC_ASSERT( core_exchange_rate.quote.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.amount > 0 );
   FC_ASSERT( core_exchange_rate.quote.amount > 0 );

   FC_ASSERT( !(flags & ~permissions ) );
   if( permissions & market_issued )
   {
      FC_ASSERT( !(permissions & ~(white_list) ) );
      FC_ASSERT( !(permissions & ~(override_authority) ) );
      FC_ASSERT( !(permissions & ~(halt_transfer) ) );
   }
}

void asset_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( permissions <= market_issued );
   FC_ASSERT( flags <= market_issued );
   if( core_exchange_rate )
   {
      FC_ASSERT( core_exchange_rate->base.amount >= share_type(0) );
      FC_ASSERT( core_exchange_rate->base.asset_id == asset_id_type() );
      FC_ASSERT( core_exchange_rate->quote.amount >= share_type(0) );
      FC_ASSERT( core_exchange_rate->quote.asset_id == asset_to_update );
   }
}

share_type asset_update_operation::calculate_fee( const fee_schedule_type& k )const
{
   return k.at( asset_update_fee_type );
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


void delegate_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( pay_rate <= 100 );
   for( auto fee : fee_schedule ) FC_ASSERT( fee.value > 0 );
   FC_ASSERT( max_block_size >= BTS_MIN_BLOCK_SIZE_LIMIT );
   FC_ASSERT( max_transaction_size >= BTS_MIN_TRANSACTION_SIZE_LIMIT );
   FC_ASSERT( block_interval_sec > 0 && block_interval_sec <= BTS_MAX_BLOCK_INTERVAL );
   FC_ASSERT( max_sec_until_expiration > block_interval_sec );
}
void delegate_update_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( pay_rate <= 100 || pay_rate == 255 );
   FC_ASSERT( fee_schedule || signing_key || pay_rate <= 100 );
   if( fee_schedule ) for( auto fee : *fee_schedule ) FC_ASSERT( fee.value > 0 );

   FC_ASSERT( max_block_size >= BTS_MIN_BLOCK_SIZE_LIMIT );
   FC_ASSERT( max_transaction_size >= BTS_MIN_TRANSACTION_SIZE_LIMIT );
   FC_ASSERT( block_interval_sec > 0 && block_interval_sec <= BTS_MAX_BLOCK_INTERVAL );
   FC_ASSERT( max_sec_until_expiration > block_interval_sec );
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

void market_order_create_operation::validate()const
{
   FC_ASSERT( amount_to_sell.asset_id != min_to_receive.asset_id );
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( amount_to_sell.amount > 0 );
   FC_ASSERT( min_to_receive.amount > 0 );
}

share_type market_order_create_operation::calculate_fee(const fee_schedule_type& k) const
{
   return k.at( market_order_fee_type );
}
} } // namespace bts::chain
