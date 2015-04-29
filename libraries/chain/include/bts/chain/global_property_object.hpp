#pragma once
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/db/object.hpp>

namespace bts { namespace chain {

   /**
    * @class global_property_object
    * @brief Maintains global state information (delegate list, current fees)
    *
    * This is an implementation detail. The values here are set by delegates to tune the blockchain parameters.
    */
   class global_property_object : public bts::db::abstract_object<global_property_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_global_property_object_type;

         chain_parameters           parameters;
         optional<chain_parameters> pending_parameters;

         vector<delegate_id_type>   active_delegates; // updated once per maintenance interval
         vector<witness_id_type>    active_witnesses; // shuffled once per round
         flat_set<account_id_type>  witness_accounts; // updated once per maintenance interval
   };

   /**
    * @class dynamic_global_property_object
    * @brief Maintains global state information (delegate list, current fees)
    *
    * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
    * current values of global blockchain properties.
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
         time_point_sec    next_maintenance_time;
   };
}}


FC_REFLECT_DERIVED( bts::chain::dynamic_global_property_object, (bts::db::object),
                    (random)
                    (head_block_number)
                    (head_block_id)
                    (time)
                    (current_witness)
                    (next_maintenance_time) )

FC_REFLECT_DERIVED( bts::chain::global_property_object, (bts::db::object),
                    (parameters)
                    (active_delegates)
                    (active_witnesses)
                  )
