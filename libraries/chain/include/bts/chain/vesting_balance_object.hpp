#pragma once

#include <algorithm>

#include <fc/uint128.hpp>

#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class vesting_balance_object;

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
         uint32_t                       vesting_seconds;
         fc::uint128_t                  coin_seconds_earned;
         fc::time_point_sec             coin_seconds_earned_last_update;

         vesting_balance_object() {}

         void _check_cap()const
         {
            fc::uint128_t coin_seconds_earned_cap = balance.amount.value;
            coin_seconds_earned_cap *= vesting_seconds;
            assert( coin_seconds_earned <= coin_seconds_earned_cap );
            return;
         }

         /**
          * Compute coin_seconds_earned.  Used during do_evaluate() to
          * non-destructively figure out whether enough coin seconds
          * are available.
          */
         fc::uint128_t compute_coin_seconds_earned( const fc::time_point_sec& t )const
         {
            assert( t >= coin_seconds_earned_last_update );
            int64_t delta_seconds = (t - coin_seconds_earned_last_update).to_seconds();
            assert( delta_seconds >= 0 );

            fc::uint128_t delta_coin_seconds = balance.amount.value;
            delta_coin_seconds *= delta_seconds;

            fc::uint128_t coin_seconds_earned_cap = balance.amount.value;
            coin_seconds_earned_cap *= vesting_seconds;

            return std::min(
               coin_seconds_earned + delta_coin_seconds,
               coin_seconds_earned_cap
               );
          }

         /**
          * Update coin_seconds_earned and
          * coin_seconds_earned_last_update fields.
          * After calling this method, it is safe to increase
          * or decrease balance.
          */
         void update_coin_seconds_earned( const fc::time_point_sec& t )
         {
            coin_seconds_earned = compute_coin_seconds_earned( t );
            coin_seconds_earned_last_update = t;
            _check_cap();
            return;
         }

         /**
          * Used internally to increase vesting balances.  Vesting
          * balances can only be increased by the blockchain.  So this
          * method should be called by paths such as cashback and
          * worker pay.  The functionality is intentionally not
          * directly accessible to users -- and it is necessary
          * for it to be inaccessible in order for the economics
          * of vesting to function as intended.
          */
         void deposit_vesting( const fc::time_point_sec& t, asset amount )
         {
            assert( amount.asset_id == asset_id_type() );
            assert( amount.asset_id == balance.asset_id );
            update_coin_seconds_earned( t );
            balance += amount;
            _check_cap();
            return;
         }

         /**
          * Used to remove a vesting balance from the VBO.  As well
          * as the balance field, coin_seconds_earned and
          * coin_seconds_earned_last_update fields are updated.
          *
          * The money doesn't "go" anywhere; the caller is responsible
          * for crediting it to the proper account.
          */
         void withdraw_vesting( const fc::time_point_sec& t, asset amount )
         {
            //
            // Failing any of these assert checks should have already tripped an FC_ASSERT in do_evaluate().
            //
            // For neatness this method is organized so fields are read-only
            // until we're sure we won't assert(), although this is not strictly necessary.
            //
            assert( amount.asset_id == balance.asset_id );
            assert( amount <= balance );

            fc::uint128_t coin_seconds_needed = amount.amount.value;
            coin_seconds_needed *= vesting_seconds;

            fc::uint128_t new_coin_seconds_earned = compute_coin_seconds_earned( t );
            assert( new_coin_seconds_earned >= coin_seconds_needed );

            coin_seconds_earned = new_coin_seconds_earned - coin_seconds_needed;
            coin_seconds_earned_last_update = t;
            balance -= amount;
            _check_cap();
            return;
         }
   };

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::vesting_balance_object, (bts::db::object),
                    (owner)
                    (balance)
                    (vesting_seconds)
                    (coin_seconds_earned)
                    (coin_seconds_earned_last_update)
                  )
