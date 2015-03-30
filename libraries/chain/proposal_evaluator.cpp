#include <bts/chain/proposal_evaluator.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

object_id_type proposal_create_evaluator::do_evaluate(const proposal_create_operation& o)
{
   const database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   FC_ASSERT( o.expiration_time <= d.head_block_time() + d.get_global_properties().maximum_proposal_lifetime,
              "Proposal expiration time is too far in the future.");

   for( const op_wrapper& op : o.proposed_ops )
      _proposed_trx.operations.push_back(op.op);
   _proposed_trx.validate();

   return object_id_type();
}

object_id_type proposal_create_evaluator::do_apply(const proposal_create_operation& o)
{
   database& d = db();

   const proposal_object& proposal = d.create<proposal_object>([&](proposal_object& proposal) {
      proposal.proposed_transaction = _proposed_trx;
      for( const operation& op : proposal.proposed_transaction.operations )
         op.visit(operation_get_required_auths(proposal.required_approvals));
      proposal.expiration_time = o.expiration_time;
   });

   return proposal.id;
}

object_id_type proposal_update_evaluator::do_evaluate(const proposal_update_operation& o)
{
   database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   for( account_id_type id : o.approvals_to_add )
      trx_state->check_authority(&id(d));
   for( account_id_type id : o.approvals_to_remove )
      trx_state->check_authority(&id(d));

   _proposal = &o.proposal(d);

   for( account_id_type id : o.approvals_to_add )
      FC_ASSERT( _proposal->required_approvals.find(id) != _proposal->required_approvals.end() );
   for( account_id_type id : o.approvals_to_remove )
      FC_ASSERT( _proposal->available_approvals.find(id) != _proposal->available_approvals.end() );

   if( o.approvals_to_remove.empty() && o.approvals_to_add.size() == _proposal->required_approvals.size() )
      _executed_proposal = true;

   return object_id_type();
}

object_id_type proposal_update_evaluator::do_apply(const proposal_update_operation& o)
{
   database& d = db();

   apply_delta_balances();
   apply_delta_fee_pools();

   if( _executed_proposal )
   {
      try {
          _processed_transaction = d.push_proposal(*_proposal);
      } catch(fc::exception& e) {
         wlog("Proposed transaction ${id} failed to apply once approved because ${reason}. Will try again when it expires.",
              ("id", o.proposal)("reason", e.to_detail_string()));
         _proposal_failed = true;
      }
   }

   //Only bother updating the proposal if we haven't removed it (if we didn't execute it, or execution failed)
   if( !_executed_proposal || _proposal_failed )
      d.modify(*_proposal, [&o](proposal_object& p) {
         for( account_id_type id : o.approvals_to_add )
         {
            p.required_approvals.erase(id);
            p.available_approvals.insert(id);
         }
         for( account_id_type id : o.approvals_to_remove )
         {
            p.available_approvals.erase(id);
            p.required_approvals.insert(id);
         }
      });

   return object_id_type();
}

object_id_type proposal_delete_evaluator::do_evaluate(const proposal_delete_operation& o)
{
   database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _proposal = &o.proposal(d);

   FC_ASSERT( _proposal->required_approvals.find(o.fee_paying_account) != _proposal->required_approvals.end() ||
              _proposal->available_approvals.find(o.fee_paying_account) != _proposal->available_approvals.end() );

   return object_id_type();
}

object_id_type proposal_delete_evaluator::do_apply(const proposal_delete_operation&)
{
   apply_delta_balances();
   apply_delta_fee_pools();

   db().remove(*_proposal);

   return object_id_type();
}

} } // bts::chain
