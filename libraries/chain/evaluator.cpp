#include<bts/chain/evaluator.hpp>
#include<bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/account_object.hpp>

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
   {
      fee_paying_account = account_id(db());
      FC_ASSERT( fee_paying_account );
      fee_asset = fee.asset_id(db());
      FC_ASSERT( fee_asset );
      fee_asset_dyn_data = fee_asset->dynamic_asset_data_id(db());
      assert( fee_asset_dyn_data );
      FC_ASSERT( get_account_balance( fee_paying_account, fee_asset ) >= fee );

      asset fee_from_pool = fee;
      if( fee.asset_id != asset_id_type() )
      {
         fee_from_pool = fee * fee_asset->core_exchange_rate;
         FC_ASSERT( fee_from_pool.asset_id == asset_id_type() );
         FC_ASSERT( fee_from_pool.amount <= fee_asset_dyn_data->fee_pool );
      }

      return fee_from_pool.amount;
   }

   /**
    *  Gets the balance of the account after all modifications that have been applied 
    *  while evaluating this operation.
    */
   asset  generic_evaluator::get_account_balance( const account_object* for_account, const asset_object* for_asset )const
   {
      auto current_balance_obj = for_account->balances(db());
      assert(current_balance_obj);
      auto current_balance = current_balance_obj->get_balance( for_asset->id );
      auto itr = delta_balance.find( make_pair(for_account,for_asset) );
      if( itr != delta_balance.end() ) 
         return asset(current_balance.amount + itr->second,for_asset->id);
      return current_balance;
   }
} }
