#include <bts/app/api.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
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

      auto db  = remote_api->database();
      auto net = remote_api->network();
      
   } 
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
   }
   return -1;
}
