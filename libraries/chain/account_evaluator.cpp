#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/key_object.hpp>
#include <algorithm>

namespace bts { namespace chain {

object_id_type account_create_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<account_create_operation>();

   auto bts_fee_paid = pay_fee( op.fee_paying_account, op.fee );
   auto bts_fee_required = op.calculate_fee( db().current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   FC_ASSERT( op.voting_key.is_relative() || db().find(op.voting_key) );
   FC_ASSERT( op.memo_key.is_relative() || db().find(op.memo_key) );

   for( auto id : op.owner.auths )
      FC_ASSERT( id.first.is_relative() || db().find<object>(id.first) );
   for( auto id : op.active.auths )
      FC_ASSERT( id.first.is_relative() || db().find<object>(id.first) );

   auto& acnt_indx = static_cast<account_index&>(db().get_index<account_object>());
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
      verify_authority( &*parent_account_itr, authority::owner );
      FC_ASSERT( op.owner.auths.find( parent_account_itr->id ) != op.owner.auths.end() );
   }

   return object_id_type();
}

object_id_type account_create_evaluator::apply( const operation& o )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   const auto& op = o.get<account_create_operation>();
   auto owner  = resolve_relative_ids( op.owner );
   auto active = resolve_relative_ids( op.active );

   const auto& bal_obj = db().create<account_balance_object>( [&]( account_balance_object& obj ){
            /* no balances right now */
   });
   const auto& dbt_obj = db().create<account_debt_object>( [&]( account_debt_object& obj ){
            /* no debts now */
   });

   const auto& new_acnt_object = db().create<account_object>( [&]( account_object& obj ){
         obj.name       = op.name;
         obj.owner      = owner;
         obj.active     = active;
         obj.memo_key   = get_relative_id(op.memo_key);
         obj.voting_key = get_relative_id(op.voting_key);
         obj.balances   = bal_obj.id;
         obj.debts      = dbt_obj.id;
   });

   return new_acnt_object.id;
}


object_id_type account_update_evaluator::evaluate( const operation& op )
{
   const auto& o = op.get<account_update_operation>();
   database&   d = db();

   auto bts_fee_paid = pay_fee( o.account, o.fee );
   FC_ASSERT( bts_fee_paid == o.calculate_fee( d.current_fee_schedule() ) );

   FC_ASSERT( !o.voting_key || o.voting_key->is_relative() || db().find(*o.voting_key) );
   FC_ASSERT( !o.memo_key || o.memo_key->is_relative() || db().find(*o.memo_key) );

   if( o.owner )
      for( auto id : o.owner->auths )
         FC_ASSERT( id.first.is_relative() || db().find<object>(id.first) );
   if( o.active )
      for( auto id : o.active->auths )
         FC_ASSERT( id.first.is_relative() || db().find<object>(id.first) );

   acnt = &o.account(d);
   if( o.owner ) FC_ASSERT( verify_authority( acnt, authority::owner ) );
   else if( o.active || o.voting_key || o.memo_key ) FC_ASSERT( verify_authority( acnt, authority::active ) );
   else if( o.vote ) FC_ASSERT( verify_signature( &acnt->voting_key(d) ) );

   if( o.vote )
   {
      std::set_difference( acnt->delegate_votes.begin(), acnt->delegate_votes.end(),
                           o.vote->begin(), o.vote->end(),
                           std::inserter( remove_votes, remove_votes.begin() ) );
      std::set_difference( o.vote->begin(), o.vote->end(),
                           acnt->delegate_votes.begin(), acnt->delegate_votes.end(),
                           std::inserter( add_votes, add_votes.begin() ) );
   }

   return object_id_type();
}
object_id_type account_update_evaluator::apply( const operation& op )
{
   const auto& o = op.get<account_update_operation>();
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
          if( o.vote ) a.delegate_votes = *o.vote;
      });
   return object_id_type();
}

} } // bts::chain
