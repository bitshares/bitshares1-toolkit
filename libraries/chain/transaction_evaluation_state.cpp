#include <bts/chain/transaction_evaluation_state.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/exceptions.hpp>

namespace bts { namespace chain {
   bool transaction_evaluation_state::check_authority( const account_object& account, authority::classification auth_class, int depth )
   {
      if( _skip_authority_check || approved_by.find(make_pair(account.id, auth_class)) != approved_by.end() )
         return true;

      FC_ASSERT( account.id.instance() != 0 || _is_proposed_trx );

      const authority* au = nullptr;
      switch( auth_class )
      {
         case authority::owner:
            au = &account.owner;
            break;
         case authority::active:
            au = &account.active;
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
                  {
                     elog("Failing authority verification due to recursion depth.");
                     return false;
                  }
                  if( check_authority( *dynamic_cast<const account_object*>( &auth_item ), auth_class, depth + 1 ) )
                  {
                     approved_by.insert( std::make_pair(auth_item.id,auth_class) );
                     total_weight += auth.second;
                  }
                  break;
               }
               case key_object_type:
               {
                  if( signed_by( auth.first ) )
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
            approved_by.insert( std::make_pair(account.id, auth_class) );
            return true;
         }
      }
      return false;
   }
   bool transaction_evaluation_state::signed_by( key_id_type id )const
   {
      assert(_trx);
      return _trx->signatures.find(id) != _trx->signatures.end();
   }

} } // namespace bts::chain
