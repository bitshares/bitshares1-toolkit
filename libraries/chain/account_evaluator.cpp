#include <bts/chain/database.hpp>
#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/key_object.hpp>
#include <algorithm>

namespace bts { namespace chain {

object_id_type account_create_evaluator::do_evaluate( const account_create_operation& op )
{ try {
   auto bts_fee_paid = pay_fee( op.registrar, op.fee );
   auto bts_fee_required = op.calculate_fee( db().current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   FC_ASSERT( is_relative(op.voting_key) || db().find_object(op.voting_key) );
   FC_ASSERT( is_relative(op.memo_key) || db().find_object(op.memo_key) );

   if( fee_paying_account->is_prime() )
   {
      FC_ASSERT( op.referrer(db()).is_prime() );
   }
   else
   {
      FC_ASSERT( op.referrer == fee_paying_account->referrer );
      FC_ASSERT( op.referrer_percent == fee_paying_account->referrer_percent, "",
                 ("op",op)
                 ("fee_paying_account->referral_percent",fee_paying_account->referrer_percent) );
   }

   for( auto id : op.owner.auths )
      FC_ASSERT( is_relative(id.first) || db().find<object>(id.first) );
   for( auto id : op.active.auths )
      FC_ASSERT( is_relative(id.first) || db().find<object>(id.first) );
   for( auto id : op.vote )
      FC_ASSERT( db().find<object>(id) );

   auto& acnt_indx = db().get_index_type<account_index>();
   if( op.name.size() )
   {
      auto current_account_itr = acnt_indx.indices().get<by_name>().find( op.name );
      FC_ASSERT( current_account_itr == acnt_indx.indices().get<by_name>().end() );
   }

   // verify child account authority
   auto pos = op.name.find( '/' );
   if( pos != string::npos )
   {
      // TODO: lookup account by op.owner.auths[0] and verify the name
      // this should be a constant time lookup rather than log(N)
      auto parent_account_itr = acnt_indx.indices().get<by_name>().find( op.name.substr(0,pos) );
      FC_ASSERT( parent_account_itr != acnt_indx.indices().get<by_name>().end() );
      FC_ASSERT( verify_authority( &*parent_account_itr, authority::owner ) );
      FC_ASSERT( op.owner.auths.find( parent_account_itr->id ) != op.owner.auths.end() );
   }

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type account_create_evaluator::do_apply( const account_create_operation& o )
{ try {
   apply_delta_balances();
   apply_delta_fee_pools();

   auto owner  = resolve_relative_ids( o.owner );
   auto active = resolve_relative_ids( o.active );

   const auto& bal_obj = db().create<account_balance_object>( [&]( account_balance_object& obj ){
            /* no balances right now */
   });

   const auto& new_acnt_object = db().create<account_object>( [&]( account_object& obj ){
         if( fee_paying_account->is_prime() )
         {
            obj.registrar        = o.registrar;
            obj.referrer         = o.referrer;
            obj.referrer_percent = o.referrer_percent;
         }
         else
         {
            obj.registrar         = fee_paying_account->registrar;
            obj.referrer          = fee_paying_account->referrer;
            obj.referrer_percent  = fee_paying_account->referrer_percent;
         }
         obj.name             = o.name;
         obj.owner            = owner;
         obj.active           = active;
         obj.balances         = bal_obj.id;
         obj.memo_key         = get_relative_id(o.memo_key);
         obj.voting_key       = get_relative_id(o.voting_key);
         obj.votes            = o.vote;
   });

   return new_acnt_object.id;
} FC_CAPTURE_AND_RETHROW((o)) }


object_id_type account_update_evaluator::do_evaluate( const account_update_operation& o )
{
   database&   d = db();

   auto bts_fee_paid = pay_fee( o.account, o.fee, o.upgrade_to_prime );
   FC_ASSERT( bts_fee_paid == o.calculate_fee( d.current_fee_schedule() ) );

   FC_ASSERT( !o.voting_key || is_relative(*o.voting_key) || db().find_object(*o.voting_key) );
   FC_ASSERT( !o.memo_key || is_relative(*o.memo_key) || db().find_object(*o.memo_key) );

   if( o.owner )
      for( auto id : o.owner->auths )
         FC_ASSERT( is_relative(id.first) || db().find<object>(id.first) );
   if( o.active )
      for( auto id : o.active->auths )
         FC_ASSERT( is_relative(id.first) || db().find<object>(id.first) );

   acnt = &o.account(d);
   if( o.upgrade_to_prime ) FC_ASSERT( !acnt->is_prime() );
   if( o.owner ) FC_ASSERT( verify_authority( acnt, authority::owner ) );
   else if( o.active || o.voting_key || o.memo_key ) FC_ASSERT( verify_authority( acnt, authority::active ) );
   else if( o.vote ) FC_ASSERT( verify_signature( &acnt->voting_key(d) ) );

   if( o.vote )
   {
      std::set_difference( acnt->votes.begin(), acnt->votes.end(),
                           o.vote->begin(), o.vote->end(),
                           std::inserter( remove_votes, remove_votes.begin() ) );
      std::set_difference( o.vote->begin(), o.vote->end(),
                           acnt->votes.begin(), acnt->votes.end(),
                           std::inserter( add_votes, add_votes.begin() ) );
   }

   return object_id_type();
}
object_id_type account_update_evaluator::do_apply( const account_update_operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   if( remove_votes.size() || add_votes.size() )
   {
      auto core_bal = acnt->balances(db()).get_balance( asset_id_type() ).amount;
      // TODO: find all orders and add their CORE ASSET BALANCE to the ACCOUNT BALANCE
      if( core_bal.value  )
      {
         adjust_votes( remove_votes, -core_bal );
         adjust_votes( add_votes, core_bal );
      }
   }
   db().modify( *acnt, [&]( account_object& a  ){
          if( o.owner ) a.owner = *o.owner;
          if( o.active ) a.active = *o.active;
          if( o.voting_key ) a.voting_key = *o.voting_key;
          if( o.memo_key ) a.memo_key = *o.memo_key;
          if( o.vote ) a.votes = *o.vote;
          if( o.upgrade_to_prime )
          {
            a.referrer_percent = 100;
            a.referrer = a.id;
          }
      });
   return object_id_type();
}

object_id_type account_whitelist_evaluator::do_evaluate(const account_whitelist_operation& o)
{ try {
   database& d = db();

   auto bts_fee_paid = pay_fee( o.authorizing_account, o.fee );
   FC_ASSERT( bts_fee_paid >= o.calculate_fee( d.current_fee_schedule() ) );

   listed_account = &o.account_to_list(d);

   return object_id_type();
} FC_CAPTURE_AND_RETHROW( (o) ) }

object_id_type account_whitelist_evaluator::do_apply(const account_whitelist_operation& o)
{
   database& d = db();

   apply_delta_balances();
   apply_delta_fee_pools();

   d.modify(*listed_account, [&o](account_object& a) {
      if( o.new_listing & o.white_listed )
         a.whitelisting_accounts.insert(o.authorizing_account);
      else
         a.whitelisting_accounts.erase(o.authorizing_account);
      if( o.new_listing & o.black_listed )
         a.blacklisting_accounts.insert(o.authorizing_account);
      else
         a.blacklisting_accounts.erase(o.authorizing_account);
   });

   return object_id_type();
}

} } // bts::chain
