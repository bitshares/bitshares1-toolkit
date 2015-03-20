#pragma once
#include <bts/db/object.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

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
