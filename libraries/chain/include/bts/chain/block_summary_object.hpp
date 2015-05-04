#pragma once
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   /**
    *  @brief tracks minimal information about past blocks to implement TaPOS
    *  @ingroup object
    *
    *  When attempting to calculate the validity of a transaction we need to
    *  lookup a past block and check its block hash and the time it occurred
    *  so we can calculate whether the current transaction is valid and at
    *  what time it should expire.
    */
   class block_summary_object : public abstract_object<block_summary_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_block_summary_object_type;

         block_id_type block_id;
         fc::time_point_sec timestamp;
   };

} }

FC_REFLECT_DERIVED( bts::chain::block_summary_object, (bts::db::object), (block_id) )
