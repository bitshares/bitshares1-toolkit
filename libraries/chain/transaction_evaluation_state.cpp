#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/exceptions.hpp>

namespace bts { namespace chain { 
   bool transaction_evaluation_state::check_authority( const address_authority& auth )const
   {
      if( _skip_signature_check ) return true;
      uint32_t count = 0;
      for( auto a : auth.addresses )
      {
         count += (signed_by.find( a ) != signed_by.end());
         if( count >= auth.required ) return true;
      }
      return false;
   }

   void transaction_evaluation_state::withdraw_from_account( account_id_type account_id, const asset& what )
   { try {
       FC_ASSERT( what.amount > 0 );
       auto asset_obj = _db->get<asset_object>(what.asset_id);
       FC_ASSERT( asset_obj );
       auto from_account = _db->get<account_object>( account_id );
       FC_ASSERT( from_account );

       if( !check_authority( from_account->active )  )
          FC_CAPTURE_AND_THROW( missing_signature, (from_account->active) );

       auto const_acc_balances = _db->get<account_balance_object>( from_account->balances );
       FC_ASSERT( const_acc_balances );
       FC_ASSERT( const_acc_balances->get_balance(what.asset_id) >= what );
       auto mutable_balance = _db->get_mutable<account_balance_object>( from_account->balances );
       mutable_balance->sub_balance( what );

       if( asset_obj->issuer == 0 )
          adjust_votes( from_account->delegate_votes, -what.amount );

   } FC_CAPTURE_AND_RETHROW( (account_id)(what) ) }

   void transaction_evaluation_state::deposit_to_account( account_id_type account_id, const asset& what )
   { try {
       auto asset_obj = _db->get<asset_object>(what.asset_id);
       FC_ASSERT( asset_obj );
       auto acc = _db->get<account_object>( account_id );
       FC_ASSERT( what.amount > 0 );
       FC_ASSERT( acc );
       auto mutable_balance = _db->get_mutable<account_balance_object>( acc->balances );
       mutable_balance->add_balance( what );

       if( asset_obj->issuer == 0 )
          adjust_votes( acc->delegate_votes, what.amount );
   } FC_CAPTURE_AND_RETHROW( (account_id)(what) ) }

   void transaction_evaluation_state::adjust_votes( const vector<delegate_id_type>& delegates, share_type amount )
   { try {
      for( auto d : delegates )
      {
         auto delegate_obj = _db->get<delegate_object>(d);
         FC_ASSERT( delegate_obj );
         auto delegate_vote_obj = _db->get_mutable<delegate_vote_object>(delegate_obj->vote);
         FC_ASSERT( delegate_vote_obj );
         delegate_vote_obj->total_votes += amount;
      }
   } FC_CAPTURE_AND_RETHROW( (delegates)(amount) ) }

} } // namespace bts::chain
