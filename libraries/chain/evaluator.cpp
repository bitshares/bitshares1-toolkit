#include<bts/chain/evaluator.hpp>
#include<bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/delegate_object.hpp>

namespace bts { namespace chain {
   database& generic_evaluator::db()const { return trx_state->db(); }
   object_id_type generic_evaluator::start_evaluate( transaction_evaluation_state& eval_state, const operation& op, bool apply )
   {
      trx_state   = &eval_state;
      auto result = evaluate( op );
      if( apply ) result = this->apply( op );
      return result;
   }
   share_type generic_evaluator::pay_fee( account_id_type account_id, asset fee )
   { try {
      FC_ASSERT( fee.amount >= 0 );
      fee_paying_account = account_id(db());
      FC_ASSERT( fee_paying_account );

      FC_ASSERT( verify_authority( fee_paying_account, authority::active ) );

      fee_asset = fee.asset_id(db());
      FC_ASSERT( fee_asset );
      fee_asset_dyn_data = fee_asset->dynamic_asset_data_id(db());
      assert( fee_asset_dyn_data );
      FC_ASSERT( get_balance( fee_paying_account, fee_asset ) >= fee );

      asset fee_from_pool = fee;
      if( fee.asset_id != asset_id_type() )
      {
         fee_from_pool = fee * fee_asset->core_exchange_rate;
         FC_ASSERT( fee_from_pool.asset_id == asset_id_type() );
         FC_ASSERT( fee_from_pool.amount <= fee_asset_dyn_data->fee_pool - fees_paid[fee_asset].from_pool );
         fees_paid[fee_asset].from_pool += fee_from_pool.amount;
      }
      adjust_balance( fee_paying_account, fee_asset, -fee.amount );
      fees_paid[fee_asset].to_issuer += fee.amount;

      return fee_from_pool.amount;
   } FC_CAPTURE_AND_RETHROW( (account_id)(fee) ) }

   bool generic_evaluator::verify_authority( const account_object* a, authority::classification c )
   {
       return trx_state->check_authority( a, c );
   }

   void generic_evaluator::adjust_balance( const account_object* for_account, const asset_object* for_asset, share_type delta )
   {
      delta_balance[for_account][for_asset] += delta;
   }
   /**
    *  Gets the balance of the account after all modifications that have been applied 
    *  while evaluating this operation.
    */
   asset  generic_evaluator::get_balance( const account_object* for_account, const asset_object* for_asset )const
   {
      auto current_balance_obj = for_account->balances(db());
      assert(current_balance_obj);
      auto current_balance = current_balance_obj->get_balance( for_asset->id );
      auto itr = delta_balance.find( for_account );
      if( itr == delta_balance.end() ) return current_balance;
      auto aitr = itr->second.find( for_asset );
      if( aitr == itr->second.end() ) return current_balance;
      return asset(current_balance.amount + aitr->second,for_asset->id);
   }

   void generic_evaluator::apply_delta_balances()
   {
      for( const auto& acnt : delta_balance )
      {
         auto balances = acnt.first->balances(db());
         db().modify( 
             balances, [&]( account_balance_object* bal ){
                for( const auto& delta : acnt.second )
                {
                   if( delta.second > 0 )
                      bal->add_balance( asset(delta.second,delta.first->id) );
                   else if( delta.second < 0 )
                      bal->sub_balance( asset(-delta.second,delta.first->id) );
                }
         });
         auto itr = acnt.second.find( asset_id_type()(db()) );
         if( itr != acnt.second.end() )
         {
            adjust_votes( acnt.first->delegate_votes, itr->second );
         }
      }
   }

   void generic_evaluator::adjust_votes( const vector<delegate_id_type>& delegate_ids, share_type delta )
   {
      database& d = db();
      for( auto id : delegate_ids )
      {
         d.modify( id(d)->vote(d), [&]( delegate_vote_object* v ){
                   v->total_votes += delta;
            });
      }
   }

   void generic_evaluator::apply_delta_fee_pools()
   {
      for( const auto& fee : fees_paid )
      {
         auto dyn_asst_data = fee.first->dynamic_asset_data_id(db());
         db().modify( dyn_asst_data, [&]( asset_dynamic_data_object* dyn ){
                          dyn->fee_pool         -= fee.second.from_pool;  
                          dyn->accumulated_fees += fee.second.to_issuer;
                     });
      }
   }
   object_id_type generic_evaluator::get_relative_id( object_id_type rel_id )const
   {
      if( rel_id.space() == relative_protocol_ids )
      {
         FC_ASSERT( rel_id.instance() < trx_state->operation_results.size() );
         return trx_state->operation_results[rel_id.instance()];
      }
      return rel_id;
   }

   authority generic_evaluator::resolve_relative_ids( const authority& a )const
   {
      authority result;
      result.auths.reserve( a.auths.size() );
      result.weight_threshold = a.weight_threshold;

      for( const auto& item : a.auths )
      {
          auto id = get_relative_id( item.first );
          FC_ASSERT( id.type() == key_object_type || id.type() == account_object_type );
          result.auths[id] = item.second;
      }

      return result;
   }

} }
