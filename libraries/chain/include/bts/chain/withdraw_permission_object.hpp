#pragma once
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/generic_index.hpp>

namespace bts { namespace chain {

  /**
   * @class withdraw_permission_object
   * @brief Grants another account authority to withdraw a limited amount of funds per interval
   *
   * The primary purpose of this object is to enable recurring payments on the blockchain. An account which wishes to
   * process a recurring payment may use a @ref withdraw_permission_claim_operation to reference an object of this type
   * and withdraw up to @ref withdrawal_limit from @ref withdraw_from_account. Only @ref authorized_account may do this,
   * and it may only be done once per withdrawal period (as defined by @ref withdrawal_period_sec), even if the first
   * withdrawal in the period was less than the limit.
   */
  class withdraw_permission_object : public bts::db::abstract_object<withdraw_permission_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = withdraw_permission_object_type;

        /// The account authorizing @ref authorized_account to withdraw from it
        account_id_type    withdraw_from_account;
        /// The account authorized to make withdrawals from @ref withdraw_from_account
        account_id_type    authorized_account;
        /// The maximum amount which may be withdrawn. All withdrawals must be of this asset type
        asset              withdrawal_limit;
        /// The duration of a withdrawal period in seconds
        uint32_t           withdrawal_period_sec;
        /// The remaining number of withdrawals authorized
        uint32_t           remaining_periods;
        /// The beginning of the next withdrawal period
        time_point_sec     next_period_start_time;
  };

   struct by_from{};
   struct by_authorized{};

   /// TODO: implement boost::hash for account_id_type and switch ot hashed_non_unique
   typedef multi_index_container<
      withdraw_permission_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_from>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::withdraw_from_account> >,
         ordered_non_unique< tag<by_authorized>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::authorized_account> >
      >
   > withdraw_permission_object_multi_index_type;

   typedef generic_index<withdraw_permission_object, withdraw_permission_object_multi_index_type> withdraw_permission_index;


} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::withdraw_permission_object, (bts::db::object),
                   (withdraw_from_account)
                   (authorized_account)
                   (withdrawal_limit)
                   (withdrawal_period_sec)
                   (remaining_periods)
                   (next_period_start_time)
                 )
