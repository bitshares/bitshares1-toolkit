#include <bts/chain/operations.hpp>

namespace bts { namespace chain {

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

void transfer_operation::validate()const
{
   FC_ASSERT( from != to );
   FC_ASSERT( amount.amount > 0 );
   FC_ASSERT( fee >= 0 );
}

void  asset_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   FC_ASSERT( is_valid_symbol( symbol ) );
   FC_ASSERT( max_supply <= BTS_MAX_SHARE_SUPPLY );
   FC_ASSERT( market_fee_percent <= BTS_MAX_MARKET_FEE_PERCENT );
   FC_ASSERT( permissions <= market_issued );
   FC_ASSERT( flags <= market_issued );
   FC_ASSERT( core_exchange_rate.quote.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.asset_id == asset_id_type() );
   FC_ASSERT( core_exchange_rate.base.amount > 0 );
   FC_ASSERT( feed_producers.size() <= BTS_MAX_FEED_PRODUCERS );

   FC_ASSERT( !(flags & ~permissions ) );
   if( permissions & market_issued )
   {
      FC_ASSERT( !(permissions & ~(white_list) ) );
      FC_ASSERT( !(permissions & ~(override_authority) ) );
      FC_ASSERT( !(permissions & ~(halt_transfer) ) );
   }
   if( feed_producers.size() )
   {
      FC_ASSERT( permissions & market_issued );
      FC_ASSERT( flags & market_issued );
   }
}

} } // namespace bts::chain
