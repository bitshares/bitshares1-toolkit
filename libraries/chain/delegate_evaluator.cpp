#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type delegate_create_evaluator::do_evaluate( const delegate_create_operation& op )
{
   return object_id_type();
}

object_id_type delegate_create_evaluator::do_apply( const delegate_create_operation& op )
{
   const auto& vote_obj = db().create<vote_tally_object>( [&]( vote_tally_object& ){
         // initial vote is 0
   });

   const auto& new_del_object = db().create<delegate_object>( [&]( delegate_object& obj ){
         obj.delegate_account         = op.delegate_account;
         obj.vote                     = vote_obj.id;
   });
   return new_del_object.id;
}

} } // bts::chain
