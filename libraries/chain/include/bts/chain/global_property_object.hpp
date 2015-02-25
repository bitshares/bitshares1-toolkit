#pragma once
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>

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
   class global_property_object : public object
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = account_object_type;

         vector<delegate_id_type> active_delegates; // updated once per round
         vector<share_type>       current_fees; // indexed by fee_type, median of active delegate fee schedules
         uint8_t                  block_interval = 5; // seconds
         uint32_t                 maximum_transaction_size = 10*1024; // 10KB
         uint32_t                 maximum_block_size = 1024*1024; // 1 MB, 200KB/sec
         uint32_t                 maximum_undo_history = 1024;
         uint32_t                 maximum_time_until_expiration = 60*60*24; // 1 day
   };
}} 

FC_REFLECT_DERIVED( bts::chain::global_property_object, (bts::chain::object), 
                    (active_delegates)
                    (current_fees)
                    (block_interval)
                    (maximum_transaction_size)
                    (maximum_block_size)
                    (maximum_undo_history)
                    (maximum_time_until_expiration)
                  )
