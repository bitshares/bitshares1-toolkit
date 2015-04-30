#pragma once

#include <bts/app/api.hpp>
#include <bts/chain/address.hpp>

using namespace bts::app;
using namespace bts::chain;
using namespace bts::utilities;
using namespace std;

namespace bts { namespace wallet {

struct wallet_data
{
   flat_set<account_id_type> accounts;
   // map of key_id -> base58 private key
   map<key_id_type, string>  keys;
   // map of account_name -> base58_private_key for
   //    incomplete account regs
   map<string, string> pending_account_registrations;

   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};

/**
 *  This wallet assumes nothing about where the database server is
 *  located and performs minimal caching.  This API could be provided
 *  locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( fc::api<login_api> rapi );
      virtual ~wallet_api();
      
      string  help()const;

      optional<signed_block> get_block( uint32_t num );
      uint64_t  get_account_count()const;
      map<string,account_id_type> list_accounts( const string& lowerbound, uint32_t limit);
      vector<asset> list_account_balances( const account_id_type& id );
      vector<asset_object> list_assets( const string& lowerbound, uint32_t limit )const;
      vector<operation_history_object>  get_account_history( account_id_type id )const;
      vector<limit_order_object>        get_limit_orders( asset_id_type a, asset_id_type b, uint32_t limit )const;
      vector<short_order_object>        get_short_orders( asset_id_type a, uint32_t limit )const;
      vector<call_order_object>         get_call_orders( asset_id_type a, uint32_t limit )const;
      vector<force_settlement_object>   get_settle_orders( asset_id_type a, uint32_t limit )const;
      string  suggest_brain_key()const;

      string serialize_transaction( signed_transaction tx ) const;

      variant get_object( object_id_type id );
      account_object get_account( string account_name_or_id );
      account_id_type get_account_id( string account_name_or_id );
      asset_id_type get_asset_id( string asset_name_or_id );

      bool import_key( string account_name_or_id, string wif_key );
      string normalize_brain_key( string s );
      fc::ecc::private_key derive_private_key(
         const std::string& prefix_string, int sequence_number);
      signed_transaction create_account_with_brain_key(
         string brain_key,
         string account_name,
         string registrar_account,
         string referrer_account,
         uint8_t referrer_percent,
         bool broadcast = false
         );
      signed_transaction transfer(
         string from,
         string to,
         uint64_t amount,
         string asset_symbol,
         string memo,
         bool broadcast = false
         );

      signed_transaction sign_transaction(
         signed_transaction tx,
         bool broadcast = false
         );

      // methods that start with underscore are not incuded in API
      void _resync();
      void _start_resync_loop();
      void _resync_loop();
      global_property_object get_global_properties() const;
      dynamic_global_property_object get_dynamic_global_properties() const;

      std::map<string,std::function<string(fc::variant,const fc::variants&)> >
      _get_result_formatters() const;

      wallet_data             _wallet;

      fc::api<login_api>      _remote_api;
      fc::api<database_api>   _remote_db;
      fc::api<network_api>    _remote_net;

      fc::future<void>        _resync_loop_task;
};

} }

FC_REFLECT( bts::wallet::wallet_data,
   (accounts)
   (keys)
   (pending_account_registrations)
   (ws_server)
   (ws_user)
   (ws_password)
   );

FC_API( bts::wallet::wallet_api,
   (help)
   (list_accounts)
   (list_account_balances)
   (list_assets)
   (import_key)
   (suggest_brain_key)
   (create_account_with_brain_key)
   (transfer)
   (get_account)
   (get_account_id)
   (get_block)
   (get_account_count)
   (get_account_history)
   (get_global_properties)
   (get_dynamic_global_properties)
   (get_object)
   (normalize_brain_key)
   (get_limit_orders)
   (get_short_orders)
   (get_call_orders)
   (get_settle_orders)
   (serialize_transaction)
   (sign_transaction)
   )
