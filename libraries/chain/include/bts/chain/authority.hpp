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
      authority(){}
      template<class ...Args>
      authority(uint32_t threshhold, Args... auths)
         : weight_threshold(threshhold)
      {
         add_authorities(auths...);
      }

      enum classification
      {
         /** the key that is authorized to change owner, active, and voting keys */
         owner  = 0,
         /** the key that is able to perform normal operations */
         active = 1,
         key    = 2
      };
      void add_authority( key_id_type k, weight_type w )
      {
         auths[k] = w;
      }
      void add_authority( account_id_type k, weight_type w )
      {
         auths[k] = w;
      }
      template<typename AuthType>
      void add_authorities(AuthType k, weight_type w)
      {
         add_authority(k, w);
      }
      template<typename AuthType, class ...Args>
      void add_authorities(AuthType k, weight_type w, Args... auths)
      {
         add_authority(k, w);
         add_authorities(auths...);
      }
      uint32_t                             weight_threshold = 0;
      flat_map<object_id_type,weight_type> auths;
   };

} } // namespace bts::chain

FC_REFLECT( bts::chain::authority, (weight_threshold)(auths) )
FC_REFLECT_TYPENAME( bts::chain::authority::classification )
FC_REFLECT_ENUM( bts::chain::authority::classification, (owner)(active)(key) )
