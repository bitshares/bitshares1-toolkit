#include <bts/app/application.hpp>
#include <bts/app/plugin.hpp>

#include <bts/chain/key_object.hpp>
#include <bts/time/time.hpp>

#include <bts/account_history/account_history_plugin.hpp>

#include <fc/thread/thread.hpp>

#include <boost/filesystem/path.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

using namespace bts;

BOOST_AUTO_TEST_CASE( two_node_network )
{
   using namespace bts::chain;
   using namespace bts::app;
   try {
      fc::temp_directory app_dir;
      fc::temp_directory app2_dir;
      fc::temp_file genesis_json;
      fc::json::save_to_file(genesis_allocation(), genesis_json.path());

      bts::time::start_simulated_time(fc::time_point::now());

      bts::app::application app1;
      app1.register_plugin<bts::account_history::account_history_plugin>();
      bpo::variables_map cfg;
      cfg.emplace("p2p-endpoint", bpo::variable_value(string("127.0.0.1:3939"), false));
      cfg.emplace("genesis-json", bpo::variable_value(boost::filesystem::path(genesis_json.path()), false));
      app1.initialize(app_dir.path(), cfg);

      bts::app::application app2;
      app2.register_plugin<account_history::account_history_plugin>();
      auto cfg2 = cfg;
      cfg2.erase("p2p-endpoint");
      cfg2.emplace("p2p-endpoint", bpo::variable_value(string("127.0.0.1:4040"), false));
      cfg2.emplace("seed-node", bpo::variable_value(vector<string>{"127.0.0.1:3939"}, false));
      app2.initialize(app2_dir.path(), cfg2);

      app1.startup();
      app2.startup();
      fc::usleep(fc::milliseconds(500));

      BOOST_CHECK_EQUAL(app1.p2p_node()->get_connection_count(), 1);
      BOOST_CHECK_EQUAL(std::string(app1.p2p_node()->get_connected_peers().front().host.get_address()), "127.0.0.1");
      ilog("Connected!");

      fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
      fc::ecc::private_key genesis_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
      bts::chain::signed_transaction trx;
      trx.operations.push_back(key_create_operation({asset(), account_id_type(1), public_key_type(nathan_key.get_public_key())}));
      trx.set_expiration(app2.chain_database()->head_block_id());
      trx.validate();
      trx.signatures[key_id_type(0)] =  genesis_key.sign_compact(trx.digest());
      processed_transaction ptrx = app1.chain_database()->push_transaction(trx);
      app1.p2p_node()->broadcast(bts::net::trx_message(trx));
      key_id_type nathan_key_id = ptrx.operation_results.front().get<object_id_type>();

      fc::usleep(fc::milliseconds(250));
      BOOST_CHECK(nathan_key_id(*app2.chain_database()).key_data.get<public_key_type>() == nathan_key.get_public_key());
      ilog("Pushed transaction");

      advance_simulated_time_to(app2.chain_database()->get_next_generation_time(witness_id_type()));
      app2.p2p_node()->broadcast(bts::net::block_message(app2.chain_database()->generate_block(genesis_key, witness_id_type())));

      fc::usleep(fc::milliseconds(500));
      BOOST_CHECK_EQUAL(app1.p2p_node()->get_connection_count(), 1);
      BOOST_CHECK_EQUAL(app1.chain_database()->head_block_num(), 1);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}
