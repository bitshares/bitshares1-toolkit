#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/key_object.hpp>

#define BOOST_TEST_MODULE ChainDatabaseTests
#include <boost/test/included/unit_test.hpp>

using namespace bts::chain;

struct database_fixture {
   database db;

   database_fixture()
   {
      db.init_genesis();
      db.push_undo_state();
   }
   ~database_fixture(){}
};

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_account )
{
   signed_transaction trx;
   create_account_operation create_account;
   create_account.name = "nathan";

   fc::ecc::private_key prk = fc::ecc::private_key::generate();
   create_account.paying_account = db.get_account_index().get("init0")->id;
   key_id_type genesis_key = create_account.paying_account(db)->owner.auths.begin()->first;
   create_account.owner.add_authority(genesis_key, 123);
   create_account.active.add_authority(genesis_key, 321);
   create_account.memo_key = genesis_key;
   create_account.voting_key = genesis_key;
   create_account.registration_fee = asset();

   trx.operations.push_back(create_account);
   db.push_transaction(trx);

   const account_object* nathan_account = db.get_account_index().get("nathan");
   BOOST_REQUIRE(nathan_account);
   BOOST_CHECK(nathan_account->id.space() == protocol_ids);
   BOOST_CHECK(nathan_account->id.type() == account_object_type);
   BOOST_CHECK(nathan_account->name == "nathan");
   BOOST_CHECK(nathan_account->authorized_assets.empty());
   BOOST_CHECK(nathan_account->delegate_votes.empty());

   BOOST_REQUIRE(nathan_account->owner.auths.size() == 1);
   BOOST_CHECK(nathan_account->owner.auths.at(genesis_key) == 123);
   BOOST_REQUIRE(nathan_account->active.auths.size() == 1);
   BOOST_CHECK(nathan_account->active.auths.at(genesis_key) == 321);
   BOOST_CHECK(nathan_account->voting_key == genesis_key);
   BOOST_CHECK(nathan_account->memo_key == genesis_key);

   const account_balance_object* balances = nathan_account->balances(db);
   BOOST_REQUIRE(balances);
   BOOST_CHECK(balances->id.space() == implementation_ids);
   BOOST_CHECK(balances->id.type() == impl_account_balance_object_type);
   BOOST_CHECK(balances->balances.empty());

   const account_debt_object* debts = nathan_account->debts(db);
   BOOST_REQUIRE(debts);
   BOOST_CHECK(debts->id.space() == implementation_ids);
   BOOST_CHECK(debts->id.type() == impl_account_debt_object_type);
   BOOST_CHECK(debts->call_orders.empty());
}

BOOST_AUTO_TEST_SUITE_END()
