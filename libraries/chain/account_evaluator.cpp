#include <bts/chain/account_evaluator.hpp>
#include <bts/chain/account_index.hpp>

namespace bts { namespace chain {

object_id_type account_create_evaluator::evaluate( const operation& o ) 
{
   const auto& op = o.get<account_create_operation>();

   auto bts_fee_paid = pay_fee( op.fee_paying_account, op.fee );
   auto bts_fee_required = op.calculate_fee( db().current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   if( op.name.size() )
   {
      auto current_account = db().get_account_index().get( op.name );
      FC_ASSERT( !current_account );
   }

   // verify child account authority
   auto pos = op.name.find( '/' );
   if( pos != string::npos )
   {
      // TODO: lookup account by op.owner.auths[0] and verify the name
      // this should be a constant time lookup rather than log(N) 
      auto parent_account = db().get_account_index().get( op.name.substr(0,pos) );
      FC_ASSERT( parent_account );
      verify_authority( parent_account, authority::owner );
      FC_ASSERT( op.owner.auths.find( parent_account->id ) != op.owner.auths.end() );
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

   auto bal_obj = db().create<account_balance_object>( [&]( account_balance_object* obj ){
            /* no balances right now */
   });
   auto dbt_obj = db().create<account_debt_object>( [&]( account_debt_object* obj ){
            /* no debts now */
   });

   auto new_acnt_object = db().create<account_object>( [&]( account_object* obj ){
         obj->name       = op.name;
         obj->owner      = owner;
         obj->active     = active;
         obj->memo_key   = get_relative_id(op.memo_key);
         obj->voting_key = get_relative_id(op.voting_key);
         obj->balances   = bal_obj->id;
         obj->debts      = dbt_obj->id;
   });

   return new_acnt_object->id;
}

} } // bts::chain
