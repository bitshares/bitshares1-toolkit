#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/database.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/operation_history_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/net/node.hpp>
#include <fc/api.hpp>

namespace bts { namespace app {
   using namespace bts::chain;

   class application;

   class database_api
   {
      public:
         database_api( bts::chain::database& db );
         ~database_api();
         fc::variants                      get_objects( const vector<object_id_type>& ids )const;
         optional<signed_block>            get_block( uint32_t block_num )const;
         global_property_object            get_global_properties()const;
         dynamic_global_property_object    get_dynamic_global_properties()const;
         vector<optional<key_object>>      get_keys( const vector<key_id_type>& key_ids )const;
         vector<optional<account_object>>  get_accounts( const vector<account_id_type>& account_ids )const;
         vector<optional<asset_object>>    get_assets( const vector<asset_id_type>& asset_ids )const;

         vector<optional<account_object>>  lookup_account_names( const vector<string>& account_name )const;
         vector<optional<asset_object>>    lookup_asset_symbols( const vector<string>& asset_symbols )const;

         vector<asset>                     get_account_balances( account_id_type id, const flat_set<asset_id_type>& assets )const;
         uint64_t                          get_account_count()const;
         map<string,account_id_type>       lookup_accounts( const string& lower_bound_name, uint32_t limit )const;
         vector<operation_history_object>  get_account_history( account_id_type, operation_history_id_type stop = operation_history_id_type() )const;

         /**
          *  @return the limit orders for both sides of the book for the two assets specified up to limit number on each side.
          */
         vector<limit_order_object>        get_limit_orders( asset_id_type a, asset_id_type b, uint32_t limit )const;
         vector<short_order_object>        get_short_orders( asset_id_type a, uint32_t limit )const;
         vector<call_order_object>         get_call_orders( asset_id_type a, uint32_t limit )const;
         vector<force_settlement_object>   get_settle_orders( asset_id_type a, uint32_t limit )const;

         vector<asset_object>              list_assets( const string& lower_bound_symbol, uint32_t limit )const;

         bool                              subscribe_to_objects(  const std::function<void(const fc::variant&)>&  callback,
                                                                  const vector<object_id_type>& ids);

         bool                              unsubscribe_from_objects( const vector<object_id_type>& ids );

      private:
         /** called every time a block is applied to report the objects that were changed */
         void on_objects_changed( const vector<object_id_type>& ids );

         fc::future<void>                                              _broadcast_changes_complete;
         boost::signals2::scoped_connection                            _change_connection;
         map<object_id_type, std::function<void(const fc::variant&)> > _subscriptions;
         bts::chain::database&                                         _db;
   };

   class history_api
   {
        history_api( application& app ):_app(app){}

        /**
         *  @return all operations related to account id from the most recent until, but not including limit_id
         */
        vector<operation_history_object>  get_account_history( account_id_type id, operation_history_id_type limit_id  = operation_history_id_type() )const;

      private:
        application&              _app;
   };

   class network_api
   {
      public:
         network_api( application& a ):_app(a){}

         void                           broadcast_transaction( const signed_transaction& trx );
         void                           add_node( const fc::ip::endpoint& ep );
         std::vector<net::peer_status>  get_connected_peers() const;

         application&              _app;
   };

   class login_api
   {
      public:
         login_api( application& a );
         ~login_api();

         bool                   login( const string& user, const string& password );
         fc::api<network_api>   network()const;
         fc::api<database_api>  database()const;
         signed_transaction     sign_transaction( signed_transaction trx, const map< key_id_type, string >& wif_keys )const;
         string                 serialize_transaction( signed_transaction trx, bool hex )const;

      private:
         application&                      _app;
         optional< fc::api<database_api> > _database_api;
         optional< fc::api<network_api> >  _network_api;
   };

}}  // bts::app

FC_API( bts::app::database_api,
        (get_objects)
        (get_block)
        (get_global_properties)
        (get_dynamic_global_properties)
        (get_keys)
        (get_accounts)
        (get_assets)
        (lookup_account_names)
        (get_account_count)
        (lookup_accounts)
        (get_account_balances)
        (get_account_history)
        (lookup_asset_symbols)
        (get_limit_orders)
        (get_short_orders)
        (get_call_orders)
        (get_settle_orders)
        (list_assets)
        (subscribe_to_objects)
        (unsubscribe_from_objects)
     )
FC_API( bts::app::network_api, (broadcast_transaction)(add_node)(get_connected_peers) )
FC_API( bts::app::login_api,
   (login)
   (network)
   (database)
   (serialize_transaction)
   (sign_transaction)
   )
