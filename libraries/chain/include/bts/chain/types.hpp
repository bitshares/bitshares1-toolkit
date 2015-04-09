#pragma once
#include <fc/container/flat_fwd.hpp>
#include <fc/io/varint.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/optional.hpp>
#include <fc/safe.hpp>
#include <fc/container/flat.hpp>
#include <fc/string.hpp>
#include <memory>
#include <vector>
#include <deque>
#include <bts/chain/address.hpp>
#include <bts/db/object_id.hpp>

namespace bts { namespace chain {
   using namespace bts::db;

   using                               std::map;
   using                               std::vector;
   using                               std::unordered_map;
   using                               std::string;
   using                               std::deque;
   using                               std::shared_ptr;
   using                               std::weak_ptr;
   using                               std::unique_ptr;
   using                               std::set;
   using                               std::pair;
   using                               std::enable_shared_from_this;
   using                               std::tie;
   using                               std::make_pair;

   using                               fc::variant_object;
   using                               fc::variant;
   using                               fc::enum_type;
   using                               fc::optional;
   using                               fc::unsigned_int;
   using                               fc::signed_int;
   using                               fc::time_point_sec;
   using                               fc::time_point;
   using                               fc::safe;
   using                               fc::flat_map;
   using                               fc::flat_set;
   using                               fc::static_variant;

   typedef fc::ecc::private_key        private_key_type;

   /**
    *
    */
   enum asset_issuer_permission_flags
   {
      charge_market_fee   = 0x01,
      white_list          = 0x02,
      halt_transfer       = 0x04,
      override_authority  = 0x08,
      market_issued       = 0x10
   };
   const static uint32_t ASSET_ISSUER_PERMISSION_MASK = 0x1f;

   enum reserved_spaces
   {
      relative_protocol_ids = 0,
      protocol_ids          = 1,
      implementation_ids    = 2
   };

   inline bool is_relative( object_id_type o ){ return o.space() == 0; }
   /**
    *  There are many types of fees charged by the network
    *  for different operations. These fees are published by
    *  the delegates and can change over time.
    */
   enum fee_type
   {
      key_create_fee_type,
      account_create_fee_type, ///< the cost to register the cheapest non-free account
      delegate_create_fee_type, ///< fixed fee for registering as a delegate, used to discourage frivioulous delegates
      delegate_update_fee_type, ///< fixed fee for registering as a delegate, used to discourage frivioulous delegates
      transfer_fee_type,
      limit_order_fee_type,
      short_order_fee_type,
      publish_feed_fee_type,
      gas_fee_type, /// defines the exchange rate between CORE and GAS
      max_gas_fee_type, /// defines the maximum GAS that can be provided
      asset_create_fee_type, ///< the cost to register the cheapest asset
      asset_update_fee_type, ///< the cost to modify a registered asset
      asset_issue_fee_type, ///< the cost to modify a registered asset
      asset_fund_fee_pool_fee_type, ///< the cost to add funds to an asset's fee pool
      market_fee_type, ///< a percentage charged on market orders
      transaction_fee_type, ///< a base price for every transaction
      data_fee_type, ///< a price per 1024 bytes of user data
      signature_fee_type, ///< a surcharge on transactions with more than 2 signatures.
      FEE_TYPE_COUNT ///< Sentry value which contains the number of different fee types
   };

   /**
    *  List all object types from all namespaces here so they can
    *  be easily reflected and displayed in debug output.  If a 3rd party
    *  wants to extend the core code then they will have to change the
    *  packed_object::type field from enum_type to uint16 to avoid
    *  warnings when converting packed_objects to/from json.
    */
   enum object_type
   {
      null_object_type,
      base_object_type,
      key_object_type,
      account_object_type,
      asset_object_type,
      delegate_object_type,
      limit_order_object_type,
      short_order_object_type,
      call_order_object_type,
      custom_object_type,
      script_object_type,
      data_object_type,
      proposal_object_type,
      operation_history_object_type
   };

   enum impl_object_type
   {
      impl_global_property_object_type,
      impl_dynamic_global_property_object_type,
      impl_index_meta_object_type,
      impl_asset_dynamic_data_type,
      impl_delegate_feeds_object_type,
      impl_account_balance_object_type,
      impl_account_debt_object_type,
      impl_delegate_vote_object_type,
      impl_transaction_object_type,
      impl_block_summary_object_type,
      impl_account_transaction_history_object_type
   };

   enum meta_info_object_type
   {
      meta_asset_object_type,
      meta_account_object_type
   };


   //typedef fc::unsigned_int            object_id_type;
   //typedef uint64_t                    object_id_type;
   class account_object;
   class delegate_object;
   class asset_object;
   class key_object;
   class limit_order_object;
   class short_order_object;
   class call_order_object;
   class custom_object;
   class script_object;
   class data_object;
   class proposal_object;
   class operation_history_object;


   typedef object_id< protocol_ids, key_object_type,          key_object>                          key_id_type;
   typedef object_id< protocol_ids, account_object_type,      account_object>                      account_id_type;
   typedef object_id< protocol_ids, asset_object_type,        asset_object>                        asset_id_type;
   typedef object_id< protocol_ids, delegate_object_type,     delegate_object>                     delegate_id_type;
   typedef object_id< protocol_ids, limit_order_object_type,  limit_order_object>                  limit_order_id_type;
   typedef object_id< protocol_ids, short_order_object_type,  short_order_object>                  short_order_id_type;
   typedef object_id< protocol_ids, call_order_object_type,   call_order_object>                   call_order_id_type;
   typedef object_id< protocol_ids, custom_object_type,       custom_object>                       custom_id_type;
   typedef object_id< protocol_ids, script_object_type,       script_object>                       script_id_type;
   typedef object_id< protocol_ids, data_object_type,         data_object>                         data_id_type;
   typedef object_id< protocol_ids, proposal_object_type,     proposal_object>                     proposal_id_type;
   typedef object_id< protocol_ids, operation_history_object_type,     operation_history_object>   operation_history_id_type;

   typedef object_id< relative_protocol_ids, key_object_type, key_object>           relative_key_id_type;
   typedef object_id< relative_protocol_ids, account_object_type, account_object>   relative_account_id_type;

   // implementation types
   class global_property_object;
   class dynamic_global_property_object;
   class index_meta_object;
   class delegate_vote_object;
   class asset_dynamic_data_object;
   class account_balance_object;
   class account_debt_object;
   class transaction_object;
   class delegate_feeds_object;
   class block_summary_object;
   class account_transaction_history_object;

   typedef object_id< implementation_ids, impl_global_property_object_type,  global_property_object>                    global_property_id_type;
   typedef object_id< implementation_ids, impl_dynamic_global_property_object_type,  dynamic_global_property_object>    dynamic_global_property_id_type;
   typedef object_id< implementation_ids, impl_asset_dynamic_data_type,      asset_dynamic_data_object>                 dynamic_asset_data_id_type;
   typedef object_id< implementation_ids, impl_account_balance_object_type,  account_balance_object>                    account_balance_id_type;
   typedef object_id< implementation_ids, impl_delegate_feeds_object_type,    delegate_feeds_object>                    delegate_feeds_id_type;
   typedef object_id< implementation_ids, impl_account_debt_object_type,     account_debt_object>                       account_debt_id_type;
   typedef object_id< implementation_ids, impl_delegate_vote_object_type,    delegate_vote_object>                      delegate_vote_id_type;
   typedef object_id< implementation_ids, impl_transaction_object_type,      transaction_object>                        transaction_obj_id_type;
   typedef object_id< implementation_ids, impl_block_summary_object_type,    block_summary_object>                      block_summary_id_type;

   typedef object_id< implementation_ids, 
                      impl_account_transaction_history_object_type,      
                      account_transaction_history_object>       account_transaction_history_id_type;


   typedef fc::array<char,BTS_MAX_SYMBOL_NAME_LENGTH>   symbol_type;
   typedef fc::ripemd160                                block_id_type;
   typedef fc::ripemd160                                transaction_id_type;
   typedef fc::sha256                                   digest_type;
   typedef fc::ecc::compact_signature                   signature_type;
   typedef safe<int64_t>                                share_type;
   typedef fc::sha224                                   secret_hash_type;
   typedef uint16_t                                     weight_type;

   struct fee_schedule_type
   {
       fee_schedule_type()
       {
          memset( (char*)fees.data, 0, sizeof(fees) );
       }
       void             set( uint32_t f, share_type v ){ FC_ASSERT( f < FEE_TYPE_COUNT && v.value <= uint32_t(-1) ); fees.at(f) = v.value; }
       const share_type at( uint32_t f )const { FC_ASSERT( f < FEE_TYPE_COUNT ); return fees.at(f); }
       size_t           size()const{ return fees.size(); }


       friend bool operator != ( const fee_schedule_type& a, const fee_schedule_type& b )
       {
          return a.fees != b.fees;
       }

       fc::array<uint32_t,FEE_TYPE_COUNT>    fees;
   };


   struct public_key_type
   {
       struct binary_key
       {
          binary_key():check(0){}
          uint32_t                 check;
          fc::ecc::public_key_data data;
       };

       fc::ecc::public_key_data key_data;

       public_key_type();
       public_key_type( const fc::ecc::public_key_data& data );
       public_key_type( const fc::ecc::public_key& pubkey );
       explicit public_key_type( const std::string& base58str );
       operator fc::ecc::public_key_data() const;
       operator fc::ecc::public_key() const;
       explicit operator std::string() const;
       friend bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2);
       friend bool operator == ( const public_key_type& p1, const public_key_type& p2);
       friend bool operator != ( const public_key_type& p1, const public_key_type& p2);
   };

} }  // bts::chain

namespace fc
{
    void to_variant( const bts::chain::public_key_type& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  bts::chain::public_key_type& vo );
    void to_variant( const bts::chain::fee_schedule_type& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  bts::chain::fee_schedule_type& vo );

}
FC_REFLECT( bts::chain::public_key_type, (key_data) )
FC_REFLECT( bts::chain::public_key_type::binary_key, (data)(check) );
FC_REFLECT( bts::chain::fee_schedule_type, (fees) )

FC_REFLECT_ENUM( bts::chain::object_type,
                 (null_object_type)
                 (base_object_type)
                 (key_object_type)
                 (account_object_type)
                 (asset_object_type)
                 (delegate_object_type)
                 (limit_order_object_type)
                 (short_order_object_type)
                 (call_order_object_type)
                 (custom_object_type)
                 (script_object_type)
                 (data_object_type)
                 (proposal_object_type)
                 (operation_history_object_type)
               )
FC_REFLECT_ENUM( bts::chain::impl_object_type,
                 (impl_global_property_object_type)
                 (impl_dynamic_global_property_object_type)
                 (impl_index_meta_object_type)
                 (impl_asset_dynamic_data_type)
                 (impl_delegate_feeds_object_type)
                 (impl_account_balance_object_type)
                 (impl_account_debt_object_type)
                 (impl_delegate_vote_object_type)
                 (impl_transaction_object_type)
                 (impl_block_summary_object_type)
                 (impl_account_transaction_history_object_type)
               )

FC_REFLECT_ENUM( bts::chain::meta_info_object_type, (meta_account_object_type)(meta_asset_object_type) )

FC_REFLECT_ENUM( bts::chain::fee_type, 
(key_create_fee_type)
(account_create_fee_type)
(delegate_create_fee_type)
(delegate_update_fee_type) 
(transfer_fee_type)
(limit_order_fee_type)
(short_order_fee_type)
(publish_feed_fee_type)
(gas_fee_type) 
(max_gas_fee_type)
(asset_create_fee_type)
(asset_update_fee_type) 
(asset_issue_fee_type)
(asset_fund_fee_pool_fee_type)
(market_fee_type)
(transaction_fee_type)
(data_fee_type)
(signature_fee_type)
(FEE_TYPE_COUNT)
);
