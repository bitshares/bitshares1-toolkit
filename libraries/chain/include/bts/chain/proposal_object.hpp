#pragma once

#include <bts/chain/types.hpp>
#include <bts/chain/transaction.hpp>
#include <bts/chain/transaction_evaluation_state.hpp>

#include <bts/db/generic_index.hpp>

namespace bts { namespace chain {

class proposal_object : public abstract_object<proposal_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = proposal_object_type;

      time_point_sec                expiration_time;
      optional<time_point_sec>      review_period_time;
      transaction                   proposed_transaction;
      flat_set<account_id_type>     required_active_approvals;
      flat_set<account_id_type>     available_active_approvals;
      flat_set<account_id_type>     required_owner_approvals;
      flat_set<account_id_type>     available_owner_approvals;
      flat_set<address>             available_key_approvals;

      bool is_authorized_to_execute(database* db)const;
};

struct by_expiration{};
typedef boost::multi_index_container<
   proposal_object,
   indexed_by<
      hashed_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag< by_expiration >, member< proposal_object, time_point_sec, &proposal_object::expiration_time > >
   >
> proposal_multi_index_container;
typedef generic_index<proposal_object, proposal_multi_index_container> proposal_index;

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::proposal_object, (bts::chain::object),
                    (expiration_time)(review_period_time)(proposed_transaction)(required_active_approvals)
                    (available_active_approvals)(required_owner_approvals)(available_owner_approvals) )
