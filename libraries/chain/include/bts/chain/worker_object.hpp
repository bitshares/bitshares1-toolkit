#pragma once
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

#include <bts/db/flat_index.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   /**
    * @brief Worker object contains the details of a blockchain worker. See @ref workers for details.
    */
   class worker_object : public abstract_object<worker_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id =  worker_object_type;

         /**
          * @brief Enumeration of the types of workers
          */
         enum worker_type_enum {
            /// Normal worker, who receives pay to a vesting balance
            project_worker_type,
            /// A special worker which automatically burns his pay
            refund_worker_type,
            /// Sentry value containing the count of worker types
            WORKER_TYPE_COUNT
         };

         /// ID of the account which owns this worker
         account_id_type                worker_account;
         /// ID of this worker's pay balance
         vesting_balance_id_type        balance;
         /// Time at which this worker begins receiving pay, if elected
         time_point_sec                 work_begin_date;
         /// Time at which this worker will cease to receive pay. Worker will be deleted at this time
         time_point_sec                 work_end_date;
         /// Amount in CORE this worker will be paid each day
         share_type                     daily_pay;
         /// Type of this worker. See @ref worker_type_enum for details
         worker_type_enum               type;

         /// Voting ID which represents approval of this worker
         vote_id_type                   vote_for;
         /// Voting ID which represents disapproval of this worker
         vote_id_type                   vote_against;
   };

   typedef flat_index<worker_object> worker_index;

} } // bts::chain

FC_REFLECT_TYPENAME( bts::chain::worker_object::worker_type_enum )
FC_REFLECT_ENUM( bts::chain::worker_object::worker_type_enum,
                 (project_worker_type)
                 (refund_worker_type)
                 (WORKER_TYPE_COUNT)
               )

FC_REFLECT_DERIVED( bts::chain::worker_object, (bts::db::object),
                    (worker_account)
                    (balance)
                    (work_begin_date)
                    (work_end_date)
                    (daily_pay)
                    (type)
                    (vote_for)
                    (vote_against)
                  )
