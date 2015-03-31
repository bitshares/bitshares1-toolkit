#pragma once
#include <bts/chain/types.hpp>
#include <bts/chain/database.hpp>
#include <bts/net/node.hpp>
#include <fc/api.hpp>

namespace bts { namespace app {
   using namespace bts::chain;

   class application;

   class database_api
   {
      public:
         database_api( bts::chain::database& db );
         fc::variants           get_objects( const vector<object_id_type>& ids )const;
         optional<signed_block> get_block( uint32_t block_num )const;

         bts::chain::database& _db;
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
         login_api( application& a ):_app(a){}

         bool                   login( const string& user, const string& password );
         fc::api<network_api>   network()const;
         fc::api<database_api>  database()const;
         signed_transaction     sign_transaction( signed_transaction trx, const vector< string >& wif_keys )const;

         application&                      _app;
         optional< fc::api<database_api> > _database_api;
         optional< fc::api<network_api> >  _network_api;
   };

}}  // bts::app

FC_API( bts::app::database_api, (get_objects)(get_block) )
FC_API( bts::app::network_api, (broadcast_transaction)(add_node)(get_connected_peers) )
FC_API( bts::app::login_api, (login)(network)(database) )
