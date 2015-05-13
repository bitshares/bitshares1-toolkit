#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   class worker_balance_object;
   class worker_term_object;

   /**
    * Worker object contains the worker's payment terms.
    * Including work begin/end date, vesting period, and daily pay.
    *
    * Each maintenance interval calculate votes for workers, sort
    * workers by NET votes for/against, then pay workers in order 
    * until the daily budget is consumed.   
    *
    * Pay workers to a vesting balance object that is configured with
    * vesting_period_days.
    *
    * (TIME SINCE LAST Maintenance Interval) / DAY  * daily pay == current pay
    */
   class worker_object : public abstract_object<worker_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id = worker_object_type;

         vesting_balance_id_type        balance;
         time_point_sec                 work_begin_date;
         time_point_sec                 work_end_date;
         share_type                     daily_pay; ///< BTS

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
