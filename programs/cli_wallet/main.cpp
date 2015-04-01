#include <bts/app/api.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/io/stdio.hpp>
#include <iostream>
#include <fc/rpc/cli.hpp>
#include <iomanip>


using namespace bts::app;
using namespace bts::chain;
using namespace std;

struct wallet_data
{
   flat_set<account_id_type> accounts;
   map<key_id_type, string>  keys;
   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};
FC_REFLECT( wallet_data, (accounts)(keys)(ws_server)(ws_user)(ws_password) );


/**
 *  This wallet assumes nothing about where the database server is
 *  located and performs minimal caching.  This API could be provided
 *  locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( fc::api<login_api> rapi )
      :_remote_api(rapi)
      {
         _remote_db  = _remote_api->database();
         _remote_net = _remote_api->network();
      }
      string  help()const;

      string  suggest_brain_key()const
      {
        return string("dummy");
      }

      signed_transaction create_account( string brain_key,
                                         string account_name,
                                         string pay_from_account )
      {
        wdump( (_remote_db->get_global_properties() ) );
        auto opt_account = _remote_db->lookup_account_names( {account_name} );
        wdump( (opt_account) );
        return signed_transaction();
      }

      signed_transaction transfer( string from, 
                                   string to, 
                                   uint64_t amount,
                                   string asset_symbol,
                                   string memo,
                                   bool broadcast = false )
      {
        auto opt_asset = _remote_db->lookup_asset_symbols( {asset_symbol} );
        wdump( (opt_asset) );
        return signed_transaction();
      }

      fc::api<login_api>      _remote_api;
      fc::api<database_api>   _remote_db;
      fc::api<network_api>    _remote_net;
};

FC_API( wallet_api, 
        (help)
        (suggest_brain_key)
        (create_account) 
        (transfer)
       )

struct help_visitor
{
   help_visitor( std::stringstream& s ):ss(s){}
   std::stringstream& ss;
   template<typename R, typename... Args>
   void operator()( const char* name, std::function<R(Args...)>& memb )const {
      ss << std::setw(40) << std::left << fc::get_typename<R>::name() << " " << name << "( ";
      vector<string> args{ fc::get_typename<Args>::name()... };
      for( uint32_t i = 0; i < args.size(); ++i )
         ss << args[i] << (i==args.size()-1?" ":", ");
      ss << ")\n";
   }

};
string  wallet_api::help()const
{
   fc::api<wallet_api> tmp;
   std::stringstream ss;
   tmp->visit( help_visitor(ss) );
   return ss.str();
}

int main( int argc, char** argv )
{
   try {
      FC_ASSERT( argc > 1, "usage: ${cmd} WALLET_FILE", ("cmd",argv[0]) );
      wallet_data wallet;

      fc::path wallet_file(argv[1]);
      if( fc::exists( wallet_file ) )
          wallet = fc::json::from_file( wallet_file ).as<wallet_data>();

      fc::http::websocket_client client;
      auto con  = client.connect( wallet.ws_server ); 
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);
      con->closed.connect( [&](){ elog( "connection closed" ); } );

      auto remote_api = apic->get_remote_api< login_api >();
      FC_ASSERT( remote_api->login( wallet.ws_user, wallet.ws_password ) );

      auto wapiptr = std::make_shared<wallet_api>(remote_api);
      fc::api<wallet_api> wapi(wapiptr);

      auto wallet_cli = std::make_shared<fc::rpc::cli>();
      wallet_cli->format_result( "help", [&]( variant result, const fc::variants& a) {
                                    return result.get_string();
                                });
      wallet_cli->register_api( wapi );
      wallet_cli->start();
      wallet_cli->wait();
   } 
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
   }
   return -1;
}
