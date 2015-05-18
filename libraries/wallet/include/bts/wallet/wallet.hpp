#pragma once

#include <bts/app/api.hpp>
#include <bts/chain/address.hpp>

using namespace bts::app;
using namespace bts::chain;
using namespace bts::utilities;
using namespace std;

namespace bts { namespace wallet {

/**
 * This class takes a variant and turns it into an object
 * of the given type, with the new operator.
 */

object* create_object( const variant& v );

struct plain_keys
{
   map<key_id_type, string>  keys;
   fc::sha512                checksum;
};

struct wallet_data
{
   flat_set<account_id_type> accounts;

   /** encrypted keys */
   vector<char>              cipher_keys;

   // map of account_name -> base58_private_key for
   //    incomplete account regs
   map<string, string> pending_account_registrations;

   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};

namespace detail {
class wallet_api_impl;
}

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

      bool copy_wallet_file( string destination_filename );

      fc::ecc::private_key derive_private_key(
         const std::string& prefix_string, int sequence_number) const;

      optional<signed_block>            get_block( uint32_t num );
      uint64_t                          get_account_count()const;
      map<string,account_id_type>       list_accounts( const string& lowerbound, uint32_t limit);
      vector<asset>                     list_account_balances( const account_id_type& id );
      vector<asset_object>              list_assets( const string& lowerbound, uint32_t limit )const;
      vector<operation_history_object>  get_account_history( account_id_type id )const;
      vector<limit_order_object>        get_limit_orders( asset_id_type a, asset_id_type b, uint32_t limit )const;
      vector<short_order_object>        get_short_orders( asset_id_type a, uint32_t limit )const;
      vector<call_order_object>         get_call_orders( asset_id_type a, uint32_t limit )const;
      vector<force_settlement_object>   get_settle_orders( asset_id_type a, uint32_t limit )const;
      global_property_object            get_global_properties() const;
      dynamic_global_property_object    get_dynamic_global_properties() const;
      account_object                    get_account( string account_name_or_id ) const;
      account_id_type                   get_account_id( string account_name_or_id ) const;
      asset_id_type                     get_asset_id( string asset_name_or_id ) const;
      variant                           get_object( object_id_type id ) const;
      void                              get_wallet_filename() const;

      bool    is_new()const;
      bool    is_locked()const;
      void    lock();
      void    unlock( string password );
      void    set_password( string password );

      string  help()const;

      bool    load_wallet_file( string wallet_filename = "" );
      void    save_wallet_file( string wallet_filename = "" );
      void    set_wallet_filename( string wallet_filename );
      string  suggest_brain_key()const;

      string serialize_transaction( signed_transaction tx ) const;

      bool import_key( string account_name_or_id, string wif_key );
      string normalize_brain_key( string s ) const;

      signed_transaction register_account( string name, 
                                           public_key_type owner, 
                                           public_key_type active,
                                           string  registrar_account,
                                           string  referrer_account,
                                           uint8_t referrer_percent,
                                           bool broadcast = false );

      signed_transaction update_account_active_authority( string name,
                                                          map<string,uint16_t> active_authority,
                                                          bool broadcast = false );
      /**
       *  Upgrades an account to prime status.
       */
      signed_transaction upgrade_account( string name, bool broadcast );

      signed_transaction create_account_with_brain_key(
         string brain_key,
         string account_name,
         string registrar_account,
         string referrer_account,
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

      signed_transaction sell_asset( string seller_account,
                                     uint64_t amount_to_sell,
                                     string   symbol_to_sell,
                                     uint64_t min_to_receive,
                                     string   symbol_to_receive,
                                     uint32_t timeout_sec = 0,
                                     bool     fill_or_kill = false,
                                     bool     broadcast = false );

      signed_transaction create_asset( string issuer, 
                                       string symbol, 
                                       uint8_t precision, 
                                       asset_object::asset_options common,
                                       fc::optional<asset_object::bitasset_options> bitasset_opts,
                                       bool broadcast = false );

      signed_transaction issue_asset( uint64_t amount, 
                                      string symbol, 
                                      string to_account,
                                      string memo,
                                      bool broadcast = false );

      signed_transaction sign_transaction(
         signed_transaction tx,
         bool broadcast = false
         );

      void _start_resync_loop();
      std::map<string,std::function<string(fc::variant,const fc::variants&)> >
      _get_result_formatters() const;


      fc::signal<void(bool)> lock_changed;

      std::unique_ptr<detail::wallet_api_impl> my;
};

} }

FC_REFLECT( bts::wallet::plain_keys, (keys)(checksum) )

FC_REFLECT( bts::wallet::wallet_data,
   (accounts)
   (cipher_keys)
   (pending_account_registrations)
   (ws_server)
   (ws_user)
   (ws_password)
   );

FC_API( bts::wallet::wallet_api,
   (help)
   (is_new)
   (is_locked)
   (lock)(unlock)(set_password)
   (list_accounts)
   (list_account_balances)
   (list_assets)
   (import_key)
   (suggest_brain_key)
   (register_account)
   (upgrade_account)
   (create_account_with_brain_key)
   (sell_asset)
   (transfer)
   (create_asset)
   (issue_asset)
   (get_account)
   (get_account_id)
   (get_block)
   (get_account_count)
   (get_account_history)
   (get_global_properties)
   (get_dynamic_global_properties)
   (get_object)
   (load_wallet_file)
   (normalize_brain_key)
   (get_limit_orders)
   (get_short_orders)
   (get_call_orders)
   (get_settle_orders)
   (save_wallet_file)
   (serialize_transaction)
   (sign_transaction)
   )
