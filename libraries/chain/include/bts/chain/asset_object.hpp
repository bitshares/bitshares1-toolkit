#pragma once
#include <bts/chain/object.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/generic_index.hpp>


namespace bts { namespace chain { 
   class account_object;

   class asset_dynamic_data_object : public abstract_object<asset_dynamic_data_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_asset_dynamic_data_type;

         share_type current_supply;
         share_type accumulated_fees; // fees accumulate to be paid out over time
         share_type fee_pool;         // in core asset
   };

   class asset_object : public annotated_object<asset_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = asset_object_type;

         static bool is_valid_symbol( const string& symbol );

         bool enforce_white_list()const { return flags & white_list; }

         string                  symbol;
         account_id_type         issuer;

         share_type              max_supply         = 0; // unlimited.
         uint16_t                market_fee_percent = 0; // 10,000 = 100%
         uint16_t                issuer_permissions; 
         uint16_t                flags; 

         asset_id_type           short_backing_asset; 

         price core_exchange_rate;    // base asset vs this asset

         /**
          *  Stores current supply, fee pool, and collected fees 
          *  in a more efficient record to serialize/modify frequently
          */
         dynamic_asset_data_id_type  dynamic_asset_data_id;

         // meta_info -> uint8_t                 precision_digits  = 0; // 0 to 10
         //   name, description, and precission 
   };

   struct by_symbol{};
   typedef multi_index_container<
      asset_object,
      indexed_by<
         hashed_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         hashed_unique< tag<by_symbol>, member<asset_object, string, &asset_object::symbol> >
      >
   > asset_object_multi_index_type;

   typedef generic_index<asset_object, asset_object_multi_index_type> asset_index;

} } // bts::chain
FC_REFLECT_DERIVED( bts::chain::asset_dynamic_data_object, (bts::chain::object),
                    (accumulated_fees)(fee_pool) )

FC_REFLECT_DERIVED( bts::chain::asset_object, 
                    (bts::chain::annotated_object<bts::chain::asset_object>), 
                    (symbol)
                    (issuer)
                    (max_supply)
                    (market_fee_percent)
                    (issuer_permissions)
                    (flags)
                    (short_backing_asset)
                    (core_exchange_rate)
                    (dynamic_asset_data_id)
                  ) 
