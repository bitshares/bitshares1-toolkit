#pragma once 

namespace bts { namespace chain {

  class market_order_object : public object
  {
     public:
        static const uint8_t space_id = protcol_ids;
        static const uint8_t type_id  = market_order_object_type;

        share_type       for_sale; ///< asset_id == sell_price.base.asset_id
        price            sell_price;
        account_id_type  seller;   
  };

  class short_order_object : public object
  {
     public:
        account_id_type  seller;   
        share_type available_collateral; ///< asset_id == sell_price.base.asset_id
        uint16_t   interest_rate; ///< in units of 0.001% APR
        price      limit_price; ///< the feed price at which the order will be canceled

  };

  class call_order_object : public object
  { 
     public:
        account_id_type  seller;   
        price            call_price;
        share_type       debt; // asset_id = call_price.quote.asset_id
        share_type       collateral; // asset_id = call_price.quote.asset_id
  };

  class call_order_index : public object
  {
     public:
         typedef multi_index_container< 
            call_order_object,
            ordered_by<  
               hashed_unique< tag<order_id>, 
                  member< object, object_id_type, &object::id > >,
               ordered_unique< tag<price_index>, 
                  composite_key< call_order_object, 
                     member< call_order_object, price, &market_order_object::call_price>,
                     member< object, object_id_type, &object::id>
                  >
            >
         > call_order_index_type;

  };

  class market_order_index : public index 
  {
     public:
         struct order_id{};
         struct price_index{};
         typedef multi_index_container< 
            market_order_object,
            ordered_by<  
               hashed_unique< tag<order_id>, 
                  member< object, object_id_type, &object::id > >,
               ordered_unique< tag<price_index>, 
                  composite_key< market_order_object, 
                     member< market_order_object, price, &market_order_object::sell_price>,
                     member< object, object_id_type, &object::id>
                  >
            >
         > market_order_index_type;
  };


} }
