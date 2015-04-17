#include <bts/app/application.hpp>
#include <bts/app/plugin.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>

#include <bts/account_history/account_history_plugin.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

using namespace bts;

BOOST_AUTO_TEST_CASE( two_node_network )
{
   using namespace bts::chain;
   try {
      fc::temp_directory app_dir;
      fc::temp_directory app2_dir;

      start_simulated_time(fc::time_point::now());

      bts::app::application app;
      app.register_plugin<bts::account_history::account_history_plugin>();
      app.configure(app_dir.path());
      app.configuration()["daemon"] = bts::app::application::daemon_configuration({fc::ip::endpoint::from_string("127.0.0.1:3939")});
      app.apply_configuration();

      bts::app::application app2;
      app2.register_plugin<account_history::account_history_plugin>();
      app2.configure(app2_dir.path());
      app2.configuration()["daemon"] = bts::app::application::daemon_configuration({fc::ip::endpoint::from_string("127.0.0.1:4040"),
                                                                                    {fc::ip::endpoint::from_string("127.0.0.1:3939")}});
      app2.apply_configuration();
      fc::usleep(fc::milliseconds(500));

      BOOST_CHECK_EQUAL(app.p2p_node()->get_connection_count(), 1);
      BOOST_CHECK_EQUAL(std::string(app.p2p_node()->get_connected_peers().front().host.get_address()), "127.0.0.1");
      ilog("Connected!");

      fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
      fc::ecc::private_key genesis_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
      bts::chain::signed_transaction trx;
      trx.operations.push_back(key_create_operation({account_id_type(1), asset(), public_key_type(nathan_key.get_public_key())}));
      trx.set_expiration(app2.chain_database()->head_block_id());
      trx.validate();
      trx.signatures.push_back(genesis_key.sign_compact(trx.digest()));
      processed_transaction ptrx = app.chain_database()->push_transaction(trx);
      app.p2p_node()->broadcast(bts::net::trx_message(trx));
      key_id_type nathan_key_id = ptrx.operation_results.front().get<object_id_type>();

      fc::usleep(fc::milliseconds(250));
      BOOST_CHECK(nathan_key_id(*app2.chain_database()).key_data.get<public_key_type>() == nathan_key.get_public_key());
      ilog("Pushed transaction");

      advance_simulated_time_to(app2.chain_database()->get_next_generation_time(witness_id_type()));
      app2.p2p_node()->broadcast(bts::net::block_message(app2.chain_database()->generate_block(genesis_key, witness_id_type())));

      fc::usleep(fc::milliseconds(500));
      BOOST_CHECK_EQUAL(app.p2p_node()->get_connection_count(), 1);
      BOOST_CHECK_EQUAL(app.chain_database()->head_block_num(), 1);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}
