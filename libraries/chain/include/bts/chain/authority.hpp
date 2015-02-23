#pragma once
#include <fc/io/varint.hpp>
#include <bts/chain/types.hpp>

namespace bts { namespace chain {

   /**
    *  @class authority
    *  @brief Identifies a weighted set of keys and accounts that must approve operations.
    */
   struct authority
   {
      enum classification
      {
         /** the key that is authorized to change owner, active, and voting keys */
         owner  = 0,
         /** the key that is able to perform normal operations */
         active = 1,
         /** a key that is only authorized to change voting behavior */
         voting = 2,
         key    = 3
      };
      uint32_t                             weight_threshold = 0;
      flat_map<object_id_type,weight_type> auths;
   };

} } // namespace bts::chain

FC_REFLECT( bts::chain::authority, (weight_threshold)(auths) )
FC_REFLECT_ENUM( bts::chain::authority::classification, (owner)(active)(voting)(key) )
