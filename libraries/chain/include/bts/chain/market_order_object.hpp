#pragma once 
#include <bts/chain/object.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/authority.hpp>
#include <bts/chain/asset.hpp>
#include <bts/chain/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace bts { namespace chain {

  class market_order_object : public abstract_object<market_order_object>
  {
     public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = market_order_object_type;

        share_type       for_sale; ///< asset_id == sell_price.base.asset_id
        price            sell_price;
        account_id_type  seller;   
  };

  struct by_id;
  struct by_price;
  typedef multi_index_container< 
     market_order_object,
     indexed_by<  
        hashed_unique< tag<by_id>, 
           member< object, object_id_type, &object::id > >,
        ordered_unique< tag<by_price>, 
           composite_key< market_order_object, 
              member< market_order_object, price, &market_order_object::sell_price>,
              member< object, object_id_type, &object::id>
           >
        >
     >
  > market_order_multi_index_type;

  typedef generic_index<market_order_object, market_order_multi_index_type> market_order_index;

#if 0
  class short_order_object : public object
  {
     public:
        account_id_type  seller;   
        share_type       available_collateral; ///< asset_id == sell_price.base.asset_id
        uint16_t         interest_rate; ///< in units of 0.001% APR
        price            limit_price; ///< the feed price at which the order will be canceled

  };

  class call_order_object : public object
  { 
     public:
        account_id_type  seller;   
        price            call_price;
        share_type       debt; // asset_id = call_price.quote.asset_id
        share_type       collateral; // asset_id = call_price.quote.asset_id
        time_point_sec   expiration;
  };

  class call_order_index : public object
  {
     public:
        struct expiration;
        struct price_index;
        struct order_id;

         typedef multi_index_container< 
            call_order_object,
            ordered_by<  
               hashed_unique< tag<order_id>, 
                  member< object, object_id_type, &object::id > >,
               ordered_unique< tag<price_index>, 
                  composite_key< call_order_object, 
                     member< call_order_object, price, &call_order_object::call_price>,
                     member< object, object_id_type, &object::id>
                  >,
               ordered_non_unique< tag<expiration>, 
                     member< call_order_object, time_point_sec, &call_order_object::expiration>
                  >
            >
         > call_order_index_type;

  };

  class market_order_index : public index 
  {
     public:
  };
#endif

} }

FC_REFLECT_DERIVED( bts::chain::market_order_object, 
                    (bts::chain::object), 
                    (for_sale)(sell_price)(seller) 
                  )
                    
