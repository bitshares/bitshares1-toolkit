#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_index.hpp>
#include <bts/chain/asset_index.hpp>
#include <bts/chain/key_object.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_account )
{
   try {
      trx.operations.push_back(make_account());
      trx.signatures.push_back(fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis"))).sign_compact(fc::digest(trx)));
      trx.validate();
      db.push_transaction(trx, ~0);

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
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer )
{
   try {
      account_id_type genesis_account;
      asset genesis_balance = genesis_account(db)->balances(db)->get_balance(asset_id_type());

      trx.operations.push_back(make_account());
      trx.validate();
      db.push_transaction(trx, ~0);

      trx = signed_transaction();
      const account_object* nathan_account = db.get_account_index().get("nathan");
      trx.operations.push_back(transfer_operation({genesis_account,
                                                   nathan_account->id,
                                                   asset(10000),
                                                   asset(),
                                                   vector<char>()
                                                  }));
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );

      asset fee = trx.operations.front().get<transfer_operation>().fee;
      trx.validate();
      db.push_transaction(trx, ~0);

      BOOST_CHECK(genesis_account(db)->balances(db)->get_balance(asset_id_type()).amount == genesis_balance.amount -
                                                                                            10000 - fee.amount);
      genesis_balance = genesis_account(db)->balances(db)->get_balance(asset_id_type());

      BOOST_REQUIRE(nathan_account);
      const account_balance_object* nathan_balances = nathan_account->balances(db);
      BOOST_REQUIRE(nathan_balances);
      BOOST_CHECK(nathan_balances->get_balance(asset_id_type()) == asset(10000));

      trx = signed_transaction();
      trx.operations.push_back(transfer_operation({nathan_account->id,
                                                   genesis_account,
                                                   asset(2000),
                                                   asset(),
                                                   vector<char>()
                                                  }));
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );

      fee = trx.operations.front().get<transfer_operation>().fee;
      trx.validate();
      db.push_transaction(trx, ~0);

      BOOST_CHECK(genesis_account(db)->balances(db)->get_balance(asset_id_type()).amount == genesis_balance.amount + 2000);
      BOOST_CHECK(nathan_balances->get_balance(asset_id_type()) == asset(10000 - 2000 - fee.amount));

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_asset )
{
   try {
      asset_id_type test_asset_id = db.get_asset_index().get_next_available_id();
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = "TEST";
      creator.max_supply = 100000;
      creator.precision = 2;
      creator.market_fee_percent = 1;
      creator.permissions = 0;
      creator.flags = 0;
      creator.core_exchange_rate = price({asset(2),asset(1)});
      creator.short_backing_asset = asset_id_type();
      trx.operations.push_back(std::move(creator));
      db.push_transaction(trx, ~0);

      const asset_object* test_asset = test_asset_id(db);
      BOOST_REQUIRE(test_asset);
      BOOST_REQUIRE(test_asset->issuer(db));
      BOOST_CHECK(test_asset->symbol == "TEST");
      BOOST_CHECK(asset(1, test_asset_id) * test_asset->core_exchange_rate == asset(2));
      BOOST_CHECK(!test_asset->enforce_white_list());
      BOOST_CHECK(test_asset->max_supply == 100000);
      BOOST_CHECK(test_asset->short_backing_asset == asset_id_type());
      BOOST_CHECK(test_asset->market_fee_percent == 1);

      int test_num = 0;
      ilog("Test duplicate symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().issuer = account_id_type(9999999);
      ilog("Test nonexistent issuer");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().issuer = account_id_type();
      trx.operations.back().get<asset_create_operation>().max_supply = -1;
      ilog("Test negative max supply");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().max_supply = 0;
      ilog("Test zero max supply");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().max_supply = 100000;
      trx.operations.back().get<asset_create_operation>().symbol = "A";
      ilog("Test single-letter symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = "qqq";
      ilog("Test lower-case symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = "11";
      ilog("Test two-digit symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = "AB CD";
      ilog("Test symbol with space");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      ilog("Test over-length symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().core_exchange_rate = price({asset(-100),asset(1)});
      ilog("Test negative core_exchange_rate");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().core_exchange_rate = price({asset(1),asset(1)});
      trx.operations.back().get<asset_create_operation>().short_backing_asset = db.get_asset_index().get_next_available_id();
      ilog("Test self-backing asset");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      trx.operations.back().get<asset_create_operation>().short_backing_asset = asset_id_type(1000000);
      ilog("Test nonexistent backing asset");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
