#pragma once

#include <algorithm>

#include <fc/static_variant.hpp>
#include <fc/uint128.hpp>

#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class vesting_balance_object;

   struct vesting_policy_context
   {
      vesting_policy_context(
         asset _balance,
         fc::time_point_sec _now,
         asset _amount )
         : balance( _balance ), now( _now ), amount( _amount ) {}
   
      asset balance;
      fc::time_point_sec now;
      asset amount;
   };

   /**
    * Linear vesting balance.
    */
   struct linear_vesting_policy
   {
      uint32_t                          vesting_seconds; // must be greater than zero
      fc::time_point_sec                begin_date;
      share_type                        begin_balance;   // same asset as balance
      share_type                        total_withdrawn; // same asset as balance

      asset get_allowed_withdraw( const vesting_policy_context& ctx )const;
      bool is_deposit_allowed( const vesting_policy_context& ctx )const;
      bool is_withdraw_allowed( const vesting_policy_context& ctx )const;
      void on_deposit( const vesting_policy_context& ctx );
      void on_withdraw( const vesting_policy_context& ctx );
   };

   struct cdd_vesting_policy
   {
      uint32_t                       vesting_seconds;
      fc::uint128_t                  coin_seconds_earned;
      fc::time_point_sec             coin_seconds_earned_last_update;

      /**
       * Compute coin_seconds_earned.  Used to
       * non-destructively figure out how many coin seconds
       * are available.
       */
      fc::uint128_t compute_coin_seconds_earned( const vesting_policy_context& ctx )const;

      /**
       * Update coin_seconds_earned and
       * coin_seconds_earned_last_update fields; called by both
       * on_deposit() and on_withdraw().
       */
      void update_coin_seconds_earned( const vesting_policy_context& ctx );

      asset get_allowed_withdraw( const vesting_policy_context& ctx )const;
      bool is_deposit_allowed( const vesting_policy_context& ctx )const;
      bool is_withdraw_allowed( const vesting_policy_context& ctx )const;
      void on_deposit( const vesting_policy_context& ctx );
      void on_withdraw( const vesting_policy_context& ctx );
   };

   typedef fc::static_variant<
      linear_vesting_policy,
      cdd_vesting_policy
      > vesting_policy;

   /**
    * Timelocked balance object is a balance that is locked by the
    * blockchain for a period of time.
    */
   class vesting_balance_object : public abstract_object<vesting_balance_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = vesting_balance_object_type;

         account_id_type                owner;
         asset                          balance;
         vesting_policy                 policy;

         vesting_balance_object() {}

         /**
          * Used to increase existing vesting balances.
          */
         void deposit( const fc::time_point_sec& now, const asset& amount );
         bool is_deposit_allowed( const fc::time_point_sec& now, const asset& amount )const;

         /**
          * Used to remove a vesting balance from the VBO.  As well
          * as the balance field, coin_seconds_earned and
          * coin_seconds_earned_last_update fields are updated.
          *
          * The money doesn't "go" anywhere; the caller is responsible
          * for crediting it to the proper account.
          */
         void withdraw( const fc::time_point_sec& now, const asset& amount );
         bool is_withdraw_allowed( const fc::time_point_sec& now, const asset& amount )const;
   };

} } // bts::chain

FC_REFLECT( bts::chain::linear_vesting_policy,
   (vesting_seconds)
   (begin_date)
   (begin_balance)
   (total_withdrawn)
)

FC_REFLECT( bts::chain::cdd_vesting_policy,
   (vesting_seconds)
   (coin_seconds_earned)
   (coin_seconds_earned_last_update)
)

FC_REFLECT_DERIVED( bts::chain::vesting_balance_object, (bts::db::object),
   (owner)
   (balance)
   (policy)
)
