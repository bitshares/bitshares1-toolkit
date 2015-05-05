#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class worker_balance_object;
   class worker_term_object;

   /**
    * Worker balance object contains the worker's running balance,
    * since it is updated frequently, it contains a minimal set of
    * information.
    */
   class worker_balance_object : public abstract_object<worker_balance_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = worker_balance_object_type;

         asset                          balance;

         worker_balance_object( asset_id_type pay_asset ) :
            balance( 0, pay_asset) {}
   };

   /**
    * Worker object contains the worker's payment terms.
    * Including work begin/end date, vesting period, and daily pay.
    */
   class worker_object : public abstract_object<worker_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = worker_object_type;

         account_id_type                worker_account;
         worker_balance_id_type         balance;
         time_point_sec                 work_begin_date;
         time_point_sec                 work_end_date;
         uint16_t                       vesting_period_days;
         uint16_t                       vesting_qzn_days;
         asset                          daily_pay;

         vote_id_type                   vote_for;
         vote_id_type                   vote_against;

         worker_object() {}
   };

} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::timelocked_balance_object, (bts::db::object),
                    (owner)
                    (balance)
                    (unlock_time)
                  )

FC_REFLECT_DERIVED( bts::chain::worker_balance_object, (bts::db::object),
                    (balance)
                  )

FC_REFLECT_DERIVED( bts::chain::worker_object, (bts::db::object),
                    (worker_account)
                    (balance)
                    (work_begin_date)
                    (work_end_date)
                    (vesting_period_days)
                    (vesting_qzn_days)
                    (daily_pay)
                    (vote_for)
                    (vote_against)
                  )
