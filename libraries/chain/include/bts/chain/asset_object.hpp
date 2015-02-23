#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/asset_operations.hpp>

namespace bts { namespace chain { 
   class account_object;

   class asset_object : public object
   {
      public:
         static const uint8_t space_id = protocal_ids;
         static const uint8_t   type_id  = asset_object_type;

         static bool is_valid_symbol( const string& symbol );

         bool enforce_white_list()const { return flags & white_list; }

         void issue( share_type amount ) 
         {
            FC_ASSERT( amount > share_type(0ll) );
            current_supply += amount;
            FC_ASSERT( current_supply < max_supply );
         }

         string                  symbol;
         account_id_type         issuer;

         share_type              max_supply         = 0; // unlimited.
         uint16_t                market_fee_percent = 0; // 10,000 = 100%
         share_type              transfer_fee       = 0;       
         uint32_t                issuer_permissions; 
         uint32_t                flags; 

         /** max producers = 101 */
         vector< account_id_type >  feed_producers; 
         asset_id_type              short_backing_asset; 

         /** DYNAMIC properties such as the current supply, fee pool, etc 
          * that are updated frequently should probably be cached separately 
          * because serializing the whole asset struct could be very
          * burdensome on the undo buffer.
          */
         share_type              current_supply     = 0; 
         /** 
          *  Stores current supply, fee pool, and collected fees 
          *  in a more efficient record to serialize/modify frequently
          */
         object_id_type          dynamic_asset_data_id;

         // meta_info -> uint8_t                 precission_digits  = 0; // 0 to 10
         //   name, description, and precission 
   };
} } // bts::chain

FC_REFLECT_DERIVED( bts::chain::asset_object, 
                    (bts::chain::object), 
                    (symbol)
                    (issuer)
                    (max_supply)
                    (current_supply)
                    (market_fee_percent)
                    (transfer_fee)
                    (issuer_permissions)
                    (flags)
                    (feed_producers)
                    (short_backing_asset)
                  ) 
