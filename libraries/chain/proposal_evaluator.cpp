#include <bts/chain/proposal_evaluator.hpp>
#include <bts/chain/proposal_object.hpp>

namespace bts { namespace chain {

object_id_type proposal_create_evaluator::do_evaluate(const proposal_create_operation& o)
{
   const database& d = db();

   auto bts_fee_required = o.calculate_fee(d.current_fee_schedule());
   auto bts_fee_paid = pay_fee(o.fee_paying_account, o.fee);
   FC_ASSERT( bts_fee_paid >= bts_fee_required );

   FC_ASSERT( o.expiration_time <= d.head_block_time() + d.get_global_properties().maximum_proposal_lifetime,
              "Proposal expiration time is too far in the future.");

   return object_id_type();
}

object_id_type proposal_create_evaluator::do_apply(const proposal_create_operation& o)
{
   database& d = db();

   const proposal_object& proposal = d.create<proposal_object>([&](proposal_object& proposal) {
      for( const op_wrapper& op : o.proposed_ops )
         proposal.proposed_transaction.operations.push_back(op.op);
         proposal.expiration_time = o.expiration_time;
   });

   return proposal.id;
}

} } // bts::chain
