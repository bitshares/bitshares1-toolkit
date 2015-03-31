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
      proposal.expiration_time = o.expiration_time;

      //Populate the required approval sets
      flat_set<account_id_type> required_active;
      for( const operation& op : proposal.proposed_transaction.operations )
         op.visit(operation_get_required_auths(required_active, proposal.required_owner_approvals));
      //All accounts which must provide both owner and active authority should be omitted from the active authority set;
      //owner authority approval implies active authority approval.
      std::set_difference(required_active.begin(), required_active.end(),
                          proposal.required_owner_approvals.begin(), proposal.required_owner_approvals.end(),
                          std::inserter(proposal.required_active_approvals, proposal.required_active_approvals.begin()));
   });

   return proposal.id;
}

object_id_type proposal_update_evaluator::do_evaluate(const proposal_update_operation& o)
{
   database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _proposal = &o.proposal(d);

   for( account_id_type id : o.active_approvals_to_add )
   {
      FC_ASSERT( _proposal->required_active_approvals.find(id) != _proposal->required_active_approvals.end() );
      FC_ASSERT( trx_state->check_authority(&id(d), authority::active) );
   }
   for( account_id_type id : o.active_approvals_to_remove )
   {
      FC_ASSERT( _proposal->available_active_approvals.find(id) != _proposal->available_active_approvals.end() );
      FC_ASSERT( trx_state->check_authority(&id(d), authority::active) );
   }
   for( account_id_type id : o.owner_approvals_to_add )
   {
      FC_ASSERT( _proposal->required_owner_approvals.find(id) != _proposal->required_owner_approvals.end() );
      FC_ASSERT( trx_state->check_authority(&id(d), authority::owner) );
   }
   for( account_id_type id : o.owner_approvals_to_remove )
   {
      FC_ASSERT( _proposal->available_owner_approvals.find(id) != _proposal->available_owner_approvals.end() );
      FC_ASSERT( trx_state->check_authority(&id(d), authority::owner) );
   }

   if( o.active_approvals_to_remove.empty() && o.owner_approvals_to_remove.empty()
       && o.active_approvals_to_add.size() == _proposal->required_active_approvals.size()
       && o.owner_approvals_to_add.size() == _proposal->required_owner_approvals.size() )
      _executed_proposal = true;

   return object_id_type();
}

object_id_type proposal_update_evaluator::do_apply(const proposal_update_operation& o)
{
   database& d = db();

   apply_delta_balances();
   apply_delta_fee_pools();

   // Potential optimization: if _executed_proposal is true, we can skip the modify step and make push_proposal skip
   // signature checks. This isn't done now because I just wrote all the proposals code, and I'm not yet 100% sure the
   // required approvals are sufficient to authorize the transaction.
   d.modify(*_proposal, [&o](proposal_object& p) {
      for( account_id_type id : o.active_approvals_to_add )
      {
         p.required_active_approvals.erase(id);
         p.available_active_approvals.insert(id);
      }
      for( account_id_type id : o.active_approvals_to_remove )
      {
         p.available_active_approvals.erase(id);
         p.required_active_approvals.insert(id);
      }
      for( account_id_type id : o.owner_approvals_to_add )
      {
         p.required_owner_approvals.erase(id);
         p.available_owner_approvals.insert(id);
      }
      for( account_id_type id : o.owner_approvals_to_remove )
      {
         p.available_owner_approvals.erase(id);
         p.required_owner_approvals.insert(id);
      }
   });

   if( _executed_proposal )
   {
      try {
          _processed_transaction = d.push_proposal(*_proposal);
      } catch(fc::exception& e) {
         wlog("Proposed transaction ${id} failed to apply once approved with exception:\n----\n${reason}\n----\nWill try again when it expires.",
              ("id", o.proposal)("reason", e.to_detail_string()));
         _proposal_failed = true;
      }
   }

   return object_id_type();
}

object_id_type proposal_delete_evaluator::do_evaluate(const proposal_delete_operation& o)
{
   database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   _proposal = &o.proposal(d);

   if( o.using_owner_authority )
      FC_ASSERT( (trx_state->check_authority(&o.fee_paying_account(d), authority::owner) &&
                  (_proposal->required_owner_approvals.find(o.fee_paying_account) != _proposal->required_owner_approvals.end() ||
                   _proposal->available_owner_approvals.find(o.fee_paying_account) != _proposal->available_owner_approvals.end())),
                 "Unable to authorize removal of proposed transaction." );
   else
      FC_ASSERT( _proposal->required_active_approvals.find(o.fee_paying_account) != _proposal->required_active_approvals.end() ||
                 _proposal->available_active_approvals.find(o.fee_paying_account) != _proposal->available_active_approvals.end(),
                 "Unable to authorize removal of proposed transaction." );

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
