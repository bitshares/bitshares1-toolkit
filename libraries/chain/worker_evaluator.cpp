#include <bts/chain/database.hpp>
#include <bts/chain/worker_evaluator.hpp>
#include <bts/chain/vesting_balance_object.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

object_id_type worker_create_evaluator::do_evaluate(const worker_create_evaluator::operation_type& o)
{ try {
   database& d = db();

   FC_ASSERT(d.get(o.owner).is_prime());
   FC_ASSERT(o.work_begin_date >= d.head_block_time());

   return object_id_type();
} FC_CAPTURE_AND_RETHROW((o)) }

object_id_type worker_create_evaluator::do_apply(const worker_create_evaluator::operation_type& o)
{ try {
   database& d = db();
   vote_id_type for_id, against_id;
   d.modify(d.get_global_properties(), [&for_id, &against_id](global_property_object& p) {
      for_id = p.get_next_vote_id(vote_id_type::worker);
      against_id = p.get_next_vote_id(vote_id_type::worker);
   });

   return d.create<worker_object>([&](worker_object& w) {
      w.worker_account = o.owner;
      w.daily_pay = o.daily_pay;
      w.work_begin_date = o.work_begin_date;
      w.work_end_date = o.work_end_date;
      w.vote_for = for_id;
      w.vote_against = against_id;
      w.worker.set_which(o.initializer.which());
      w.worker.visit(worker_initialize_visitor(w, o.initializer, d));
   }).id;
} FC_CAPTURE_AND_RETHROW((o)) }

} } // bts::chain
