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
   vote_id_type vote_id;
   db().modify(db().get_global_properties(), [&vote_id](global_property_object& p) {
      vote_id = p.next_available_vote_id++;
   });

   const auto& new_del_object = db().create<delegate_object>( [&]( delegate_object& obj ){
         obj.delegate_account         = op.delegate_account;
         obj.vote_id                  = vote_id;
   });
   return new_del_object.id;
}

} } // bts::chain
