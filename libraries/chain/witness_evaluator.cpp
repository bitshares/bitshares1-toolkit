#include <bts/chain/witness_evaluator.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type witness_create_evaluator::do_evaluate( const witness_create_operation& op )
{
   database& d = db();

   auto bts_fee_paid = pay_fee( op.witness_account, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   return object_id_type();
}

object_id_type witness_create_evaluator::do_apply( const witness_create_operation& op )
{
   apply_delta_balances();
   apply_delta_fee_pools();

   const auto& vote_obj = db().create<vote_tally_object>( [&]( vote_tally_object& ){
         // initial vote is 0
   });

   const auto& new_witness_object = db().create<witness_object>( [&]( witness_object& obj ){
         obj.witness_account     = op.witness_account;
         obj.vote                = vote_obj.id;
         obj.signing_key         = op.block_signing_key;
         obj.next_secret         = op.initial_secret;
   });
   return new_witness_object.id;
}

} } // bts::chain
