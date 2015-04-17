#include <bts/chain/database.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/proposal_object.hpp>

namespace bts { namespace chain {

bool proposal_object::is_authorized_to_execute(database* db) const
{
   transaction_evaluation_state dry_run_eval(db);
   dry_run_eval._is_proposed_trx = true;
   std::transform(available_active_approvals.begin(), available_active_approvals.end(),
                  std::inserter(dry_run_eval.approved_by, dry_run_eval.approved_by.end()), [](object_id_type id) {
      return make_pair(id, authority::active);
   });
   std::transform(available_owner_approvals.begin(), available_owner_approvals.end(),
                  std::inserter(dry_run_eval.approved_by, dry_run_eval.approved_by.end()), [](object_id_type id) {
      return make_pair(id, authority::owner);
   });
   dry_run_eval.signed_by.insert(available_key_approvals.begin(), available_key_approvals.end());

   // Check all required approvals. If any of them are unsatisfied, return false.
   for( const auto& id : required_active_approvals )
      if( !dry_run_eval.check_authority(&id(*db), authority::active) )
         return false;
   for( const auto& id : required_owner_approvals )
      if( !dry_run_eval.check_authority(&id(*db), authority::owner) )
         return false;

   return true;
}

} } // bts::chain
