#include <bts/app/api.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/io/stdio.hpp>
#include <iostream>


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
      string  suggest_brain_key()const
      {

      }

      signed_transaction create_account( string brain_key,
                                         string account_name,
                                         string pay_from_account )
      {

        return signed_transaction();
      }

      signed_transaction transfer( string from, 
                                   string to, 
                                   uint64_t amount,
                                   string asset,
                                   string memo,
                                   bool broadcast = false )
      {
        return signed_transaction();
      }

      fc::api<login_api>      _remote_api;
      fc::api<database_api>   _remote_db;
      fc::api<network_api>    _remote_net;
};

FC_API( wallet_api, 
        (suggest_brain_key)
        (create_account) 
        (transfer)
       )

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

      auto remote_api = apic->get_remote_api< login_api >();
      FC_ASSERT( remote_api->login( wallet.ws_user, wallet.ws_password ) );

      auto wapi = std::make_shared<wallet_api>(remote_api);
      
      std::string line;
      while( true )
      {
         std::cout << ">>> ";
         fc::getline( fc::cin, line );
         std::cout << line <<"\n";
         try {
            if( line == "help" )
            {
               std::cout << "Commands:\n";
               std::cout << "\tquit\n";
               std::cout << "\tcreate account\n";
               std::cout << "\tdisplay account\n";
               std::cout << "\ttransfer\n";
            }
            else if( line == "quit" )
            {
               fc::json::save_to_file( fc::variant( wallet ), wallet_file );
               return 0;
            }
            else if( line == "create account" )
            {

            }
            else if( line == "display account" )
            {

            }
            else if( line == "transfer" )
            {

            }
            else 
            {
               std::cout << "Unknown Command: '" << line <<"'\n\n";
            }
         } 
         catch ( const fc::exception& e )
         {
            std::cout << e.to_detail_string() << "\n";
         }
      }
   } 
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
   }
   return -1;
}
