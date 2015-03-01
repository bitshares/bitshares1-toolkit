#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/account_index.hpp>

namespace bts { namespace chain {
/**
 *  Valid names are all lower case, start with [a-z] and may
 *  have "." or "-" in the name along with a single '/'.  The
 *  next character after a "/", "." or "-" cannot be [0-9] or
 *  another '.', '-'.  
 *
 */
bool account_create_evaluator::is_valid_name( const string& s )
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
bool account_create_evaluator::is_premium_name( const string& n )
{
   return false;
}
bool account_create_evaluator::is_cheap_name( const string& n )
{
   for( auto c : n )
   {
      if( c >= '0' && c <= '9' ) return true;
      if( c == '.' || c == '-' || c == '/' ) return true;
   }
   return false;
}

object_id_type account_create_evaluator::evaluate( const operation& o ) 
{
   const auto& op = o.get<account_create_operation>();
   FC_ASSERT( op.name.size() < 64 );

   auto bts_fee_paid = pay_fee( op.fee_paying_account, op.fee );

   auto bts_fee_required = db().current_fee( account_create_fee_type );

   if( op.name.size() )
   {
      uint32_t s = op.name.size();
      if( is_premium_name( op.name ) )    s = 2;
      else if( is_cheap_name( op.name ) ) s = 63;
      while( s <= 8 ) {  bts_fee_required *= 10; ++s; }

      auto current_account = db().get_account_index().get( op.name );
      FC_ASSERT( !current_account );
   }

   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   // TODO: verify that all authority/key object ids are valid or
   // relative 

   // verify child account authority
   auto pos = op.name.find( '/' );
   if( pos != string::npos )
   {
      // TODO: lookup account by op.owner.auths[0] and verify the name
      // this should be a constant time lookup rather than log(N) 
      auto parent_account = db().get_account_index().get( op.name.substr(0,pos) );
      FC_ASSERT( parent_account );
      verify_authority( parent_account, authority::owner );
      FC_ASSERT( op.owner.weight_threshold == 1 );
      FC_ASSERT( op.owner.auths.size() == 1 );
      FC_ASSERT( op.owner.auths.find( parent_account->id ) != op.owner.auths.end() );
   }

   return object_id_type();
}

object_id_type account_create_evaluator::apply( const operation& o ) 
{
   // TODO: verify that all relative IDs in authority checkout 
   apply_delta_balances();
   apply_delta_fee_pools();

   const auto& op = o.get<account_create_operation>();

   auto bal_obj = db().create<account_balance_object>( [&]( account_balance_object* obj ){
            /* no balances right now */
   });
   auto dbt_obj = db().create<account_debt_object>( [&]( account_debt_object* obj ){
            /* no debts now */
   });

   auto new_acnt_object = db().create<account_object>( [&]( account_object* obj ){
         obj->name       = op.name;
         obj->owner      = op.owner;
         obj->active     = op.active;
         obj->memo_key   = op.memo_key;
         obj->voting_key = op.voting_key;
         obj->balances   = bal_obj->id;
         obj->debts      = dbt_obj->id;
   });

   return new_acnt_object->id;
}

} } // bts::chain
