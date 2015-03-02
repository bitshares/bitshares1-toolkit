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
share_type transfer_operation::calculate_fee( const fee_schedule_type& schedule )const
{
   auto bts_fee_required = schedule.at( transfer_fee_type );
   bts_fee_required += share_type((memo.size() * schedule.at( data_fee_type ).value)/1024);
   return bts_fee_required;
}


} } // namespace bts::chain
