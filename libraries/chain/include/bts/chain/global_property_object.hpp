#pragma once
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {

   /**
    *  @class global_property_object
    *  @brief Maintains global state information (delegate list, current fees)
    *
    *  This is an implementation detail.  The values provided here are the
    *  median values of the delegate objects and they are updated on a
    *  limited basis.  Active delegates are updated once per round and the
    *  other properties are updated once per day.
    */
   class global_property_object : public bts::db::abstract_object<global_property_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_global_property_object_type;

         vector<delegate_id_type>   active_delegates; // updated once per round
         vector<witness_id_type>    active_witnesses; // updated once per round
         fee_schedule_type          current_fees; // indexed by fee_type, median of active delegate fee schedules
         share_type                 witness_pay                         = BTS_DEFAULT_WITNESS_PAY;
         uint8_t                    block_interval                      = BTS_DEFAULT_BLOCK_INTERVAL; // seconds
         uint32_t                   maintenance_interval                = BTS_DEFAULT_MAINTENANCE_INTERVAL;
         uint32_t                   maximum_transaction_size            = BTS_DEFAULT_MAX_TRANSACTION_SIZE;
         uint32_t                   maximum_block_size                  = BTS_DEFAULT_MAX_BLOCK_SIZE;
         uint32_t                   maximum_undo_history                = BTS_DEFAULT_MAX_UNDO_HISTORY;
         uint32_t                   maximum_time_until_expiration       = BTS_DEFAULT_MAX_TIME_UNTIL_EXPIRATION;
         uint32_t                   maximum_proposal_lifetime           = BTS_DEFAULT_MAX_PROPOSAL_LIFETIME_SEC;
         uint8_t                    maximum_asset_whitelist_authorities = BTS_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES;
         time_point_sec             next_maintenance_time;
   };

   /**
    *  @class global_property_object
    *  @brief Maintains global state information (delegate list, current fees)
    *
    *  This is an implementation detail.  The values provided here are the
    *  median values of the delegate objects and they are updated on a
    *  limited basis.  Active delegates are updated once per round and the
    *  other properties are updated once per day.
    */
   class dynamic_global_property_object : public abstract_object<dynamic_global_property_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_dynamic_global_property_object_type;

         secret_hash_type  random;
         uint32_t          head_block_number = 0;
         block_id_type     head_block_id;
         time_point_sec    time;
         witness_id_type   current_witness;
   };
}}


FC_REFLECT_DERIVED( bts::chain::dynamic_global_property_object, (bts::db::object),
                    (random)(head_block_number)(head_block_id)(time)(current_witness) )

FC_REFLECT_DERIVED( bts::chain::global_property_object, (bts::db::object),
                    (witness_pay)
                    (active_delegates)
                    (active_witnesses)
                    (current_fees)
                    (block_interval)
                    (maintenance_interval)
                    (maximum_transaction_size)
                    (maximum_block_size)
                    (maximum_undo_history)
                    (maximum_time_until_expiration)
                    (maximum_proposal_lifetime)
                    (maximum_asset_whitelist_authorities)
                    (next_maintenance_time)
                  )
