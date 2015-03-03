#include <bts/chain/delegate_evaluator.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {
object_id_type delegate_create_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<delegate_create_operation>();
   database& d = db();

   auto bts_fee_paid = pay_fee( op.delegate_account, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );
   FC_ASSERT( op.signing_key(d) != nullptr );

   return object_id_type();
}

object_id_type delegate_create_evaluator::apply( const operation& o )
{
   const auto& op = o.get<delegate_create_operation>();
   apply_delta_balances();
   apply_delta_fee_pools();

   auto vote_obj = db().create<delegate_vote_object>( [&]( delegate_vote_object* obj ){
         // initial vote is 0
   });

   auto new_del_object = db().create<delegate_object>( [&]( delegate_object* obj ){
         obj->delegate_account = op.delegate_account;
         obj->pay_rate         = op.pay_rate;
         obj->signing_key      = op.signing_key;
         obj->next_secret      = op.first_secret_hash;
         obj->fee_schedule     = op.fee_schedule;
         obj->vote             = vote_obj->id;

   });
   return object_id_type();
}


object_id_type delegate_update_evaluator::evaluate( const operation& o )
{
   const auto& op = o.get<delegate_update_operation>();
   database& d = db();
   const delegate_object* del = op.delegate_id(d);

   auto bts_fee_paid = pay_fee( del->delegate_account, op.fee );
   auto bts_fee_required = op.calculate_fee( d.current_fee_schedule() );
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   if( op.fee_schedule ) FC_ASSERT( del->fee_schedule != *op.fee_schedule );
   if( op.pay_rate <= 255 ) FC_ASSERT( op.pay_rate != del->pay_rate );
   if( op.signing_key && !op.signing_key->is_relative() ) FC_ASSERT( op.signing_key != del->signing_key );

   return object_id_type();
}

object_id_type delegate_update_evaluator::apply( const operation& o )
{
   const auto& op = o.get<delegate_update_operation>();
   apply_delta_balances();
   apply_delta_fee_pools();

   db().modify<delegate_object>( op.delegate_id(db()), [&]( delegate_object* obj ){
         if( op.pay_rate <= 100 ) obj->pay_rate     = op.pay_rate;
         if( op.signing_key     ) obj->signing_key  = get_relative_id( *op.signing_key );
         if( op.fee_schedule    ) obj->fee_schedule = *op.fee_schedule;
   });
   return object_id_type();
}


} } // bts::chain
