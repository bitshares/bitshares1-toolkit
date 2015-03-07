#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/exceptions.hpp>

namespace bts { namespace chain { 
   bool transaction_evaluation_state::check_authority( const account_object* account, authority::classification auth_class, int depth )
   {
      FC_ASSERT( account != nullptr );
      if( _skip_signature_check ) return true;
      const authority* au = nullptr;
      switch( auth_class )
      {
         case authority::owner:
            au = &account->owner;
            break;
         case authority::active:
            au = &account->active;
            break;
         default:
            FC_ASSERT( false, "Invalid Account Auth Class" );
      };

      uint32_t total_weight = 0;
      for( const auto& auth : au->auths )
      {
         if( approved_by.find( std::make_pair(auth.first,auth_class) ) != approved_by.end() )
            total_weight += auth.second;
         else 
         {
            const object& auth_item = _db->get_object( auth.first );
            switch( auth_item.id.type() )
            {
               case account_object_type:
               {
                  if( depth == BTS_MAX_SIG_CHECK_DEPTH ) 
                     return false;
                  if( check_authority( dynamic_cast<const account_object*>( &auth_item ), auth_class, depth + 1 ) )
                  {
                     approved_by.insert( std::make_pair(auth_item.id,auth_class) );
                     total_weight += auth.second;
                  }
                  break;
               }
               case key_object_type:
               {
                  auto key_obj = dynamic_cast<const key_object*>( &auth_item );
                  FC_ASSERT( key_obj );
                  if( signed_by.find( key_obj->key_address() ) != signed_by.end() )
                  {
                     approved_by.insert( std::make_pair(auth_item.id,authority::key) );
                     total_weight += auth.second;
                  }
                  break;
               }
               default:
                  FC_ASSERT( !"Invalid Auth Object Type", "type:${type}", ("type",auth_item.id.type()) );
            }
         }
         if( total_weight >= au->weight_threshold )
         {
            approved_by.insert( std::make_pair(account->id, auth_class) );
            return true;
         }
      }
      return false;
   }

   void transaction_evaluation_state::withdraw_from_account( account_id_type account_id, const asset& what )
   { try {
       /*
       FC_ASSERT( what.amount > 0 );
       auto asset_obj = _db->get<asset_object>(what.asset_id);
       FC_ASSERT( asset_obj );
       auto from_account = _db->get<account_object>( account_id );
       FC_ASSERT( from_account );

       if( !check_authority( from_account, authority::active )  )
          FC_CAPTURE_AND_THROW( missing_signature, (from_account->active) );

       auto const_acc_balances = _db->get<account_balance_object>( from_account->balances );
       FC_ASSERT( const_acc_balances );
       FC_ASSERT( const_acc_balances->get_balance(what.asset_id) >= what );
       auto mutable_balance = _db->get_mutable<account_balance_object>( from_account->balances );
       mutable_balance->sub_balance( what );

       if( asset_obj->issuer == 0 )
          adjust_votes( from_account->delegate_votes, -what.amount );
       */

   } FC_CAPTURE_AND_RETHROW( (account_id)(what) ) }

   void transaction_evaluation_state::deposit_to_account( account_id_type account_id, const asset& what )
   { try {
            /*
       auto asset_obj = _db->get<asset_object>(what.asset_id);
       FC_ASSERT( asset_obj );
       auto acc = _db->get<account_object>( account_id );
       FC_ASSERT( what.amount > 0 );
       FC_ASSERT( acc );
       auto mutable_balance = _db->get_mutable<account_balance_object>( acc->balances );
       mutable_balance->add_balance( what );

       if( asset_obj->issuer == 0 )
          adjust_votes( acc->delegate_votes, what.amount );
          */
   } FC_CAPTURE_AND_RETHROW( (account_id)(what) ) }

   void transaction_evaluation_state::adjust_votes( const vector<delegate_id_type>& delegates, share_type amount )
   { try {
       /*
      for( auto d : delegates )
      {
         auto delegate_obj = _db->get<delegate_object>(d);
         FC_ASSERT( delegate_obj );
         auto delegate_vote_obj = _db->get_mutable<delegate_vote_object>(delegate_obj->vote);
         FC_ASSERT( delegate_vote_obj );
         delegate_vote_obj->total_votes += amount;
      }
      */
   } FC_CAPTURE_AND_RETHROW( (delegates)(amount) ) }

} } // namespace bts::chain
