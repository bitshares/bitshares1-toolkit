
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>

#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>

#include <bts/app/api.hpp>
#include <bts/chain/address.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/wallet/wallet.hpp>

using namespace bts::app;
using namespace bts::chain;
using namespace bts::utilities;
using namespace bts::wallet;
using namespace std;

int main( int argc, char** argv )
{
   try {
      FC_ASSERT( argc > 1, "usage: ${cmd} WALLET_FILE", ("cmd",argv[0]) );

      fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));

      idump( (key_to_wif( genesis_private_key ) ) );
      idump( (account_id_type()) );

      fc::ecc::private_key nathan_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      idump( (key_to_wif( nathan_private_key ) ) );

      //
      // TODO:  We read wallet_data twice, once in main() to grab the
      //    socket info, again in wallet_api when we do
      //    load_wallet_file().  Seems like this could be better
      //    designed.
      //
      wallet_data wdata;
      fc::path wallet_file(argv[1]);
      if( fc::exists( wallet_file ) )
          wdata = fc::json::from_file( wallet_file ).as<wallet_data>();

      fc::http::websocket_client client;
      auto con  = client.connect( wdata.ws_server );
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);
      con->closed.connect( [&](){ elog( "connection closed" ); } );

      auto remote_api = apic->get_remote_api< login_api >();
      FC_ASSERT( remote_api->login( wdata.ws_user, wdata.ws_password ) );

      auto wapiptr = std::make_shared<wallet_api>(remote_api);
      wapiptr->set_wallet_filename( argv[1] );
      wapiptr->load_wallet_file();
      wapiptr->_start_resync_loop();

      fc::api<wallet_api> wapi(wapiptr);

      auto wallet_cli = std::make_shared<fc::rpc::cli>();
      for( auto& name_formatter : wapiptr->_get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

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
