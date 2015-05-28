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
        /// The maximum amount which may be withdrawn per period. All withdrawals must be of this asset type
        asset              withdrawal_limit;
        /// The duration of a withdrawal period in seconds
        uint32_t           withdrawal_period_sec = 0;
        /// The beginning of the next withdrawal period
        time_point_sec     next_period_start_time;
        /// The time at which this withdraw permission expires
        time_point_sec     expiration;

        /// tracks the total amount
        share_type         claimed_this_period;
        /// True if the permission may still be claimed for this period; false if it has already been used
        bool               claimable()const { return claimed_this_period < withdrawal_limit.amount; }
        asset              available_this_period( fc::time_point_sec current_time )const 
        { 
           if( current_time > next_period_start_time + withdrawal_period_sec )
              return withdrawal_limit;
           return asset( claimable() ? withdrawal_limit.amount - claimed_this_period : 0, withdrawal_limit.asset_id); 
        }

        /// Updates @ref remaining_periods and @ref next_period_start_time
        /// @return true if permission is expired; false otherwise
        /*
        bool update_period(fc::time_point_sec current_time) {
           while( remaining_periods > 0 && next_period_start_time <= current_time )
           {
              next_period_start_time += withdrawal_period_sec;
              --remaining_periods;
              claimed_this_period = 0;
           }
           return remaining_periods == 0 && next_period_start_time <= current_time;
        }
        */
   };

   struct by_from;
   struct by_authorized;
   struct by_expiration;

   typedef multi_index_container<
      withdraw_permission_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_non_unique< tag<by_from>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::withdraw_from_account> >,
         hashed_non_unique< tag<by_authorized>, member<withdraw_permission_object, account_id_type, &withdraw_permission_object::authorized_account> >,
         ordered_non_unique< tag<by_expiration>, member<withdraw_permission_object, time_point_sec, &withdraw_permission_object::expiration> >
      >
   > withdraw_permission_object_multi_index_type;

   typedef generic_index<withdraw_permission_object, withdraw_permission_object_multi_index_type> withdraw_permission_index;


} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::withdraw_permission_object, (bts::db::object),
                    (withdraw_from_account)
                    (authorized_account)
                    (withdrawal_limit)
                    (withdrawal_period_sec)
                    (next_period_start_time)
                    (expiration)
                 )
