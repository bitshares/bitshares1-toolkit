#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

#define CHECK_THROW_WITH_VALUE(op, field, value) \
{ \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception); \
}

BOOST_AUTO_TEST_CASE( create_account )
{
   try {
      trx.operations.push_back(make_account());
      trx.signatures.push_back(fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis"))).sign_compact(fc::digest(trx)));
      trx.validate();
      db.push_transaction(trx, ~0);

      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      BOOST_CHECK(nathan_account.id.space() == protocol_ids);
      BOOST_CHECK(nathan_account.id.type() == account_object_type);
      BOOST_CHECK(nathan_account.name == "nathan");
      BOOST_CHECK(nathan_account.authorized_assets.empty());
      BOOST_CHECK(nathan_account.delegate_votes.empty());

      BOOST_REQUIRE(nathan_account.owner.auths.size() == 1);
      BOOST_CHECK(nathan_account.owner.auths.at(genesis_key) == 123);
      BOOST_REQUIRE(nathan_account.active.auths.size() == 1);
      BOOST_CHECK(nathan_account.active.auths.at(genesis_key) == 321);
      BOOST_CHECK(nathan_account.voting_key == genesis_key);
      BOOST_CHECK(nathan_account.memo_key == genesis_key);

      const account_balance_object& balances = nathan_account.balances(db);
      BOOST_CHECK(balances.id.space() == implementation_ids);
      BOOST_CHECK(balances.id.type() == impl_account_balance_object_type);
      BOOST_CHECK(balances.balances.empty());

      const account_debt_object& debts = nathan_account.debts(db);
      BOOST_CHECK(debts.id.space() == implementation_ids);
      BOOST_CHECK(debts.id.type() == impl_account_debt_object_type);
      BOOST_CHECK(debts.call_orders.empty());
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer )
{
   try {
      account_id_type genesis_account;
      asset genesis_balance = genesis_account(db).balances(db).get_balance(asset_id_type());

      trx.operations.push_back(make_account());
      trx.validate();
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      db.push_transaction(trx, ~0);

      trx = signed_transaction();
      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      trx.operations.push_back(transfer_operation({genesis_account,
                                                   nathan_account.id,
                                                   asset(10000),
                                                   asset(),
                                                   vector<char>()
                                                  }));
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );

      asset fee = trx.operations.front().get<transfer_operation>().fee;
      trx.validate();
      db.push_transaction(trx, ~0);

      BOOST_CHECK(genesis_account(db).balances(db).get_balance(asset_id_type()).amount == genesis_balance.amount -
                                                                                            10000 - fee.amount);
      genesis_balance = genesis_account(db).balances(db).get_balance(asset_id_type());

      const account_balance_object& nathan_balances = nathan_account.balances(db);
      BOOST_CHECK(nathan_balances.get_balance(asset_id_type()) == asset(10000));

      trx = signed_transaction();
      trx.operations.push_back(transfer_operation({nathan_account.id,
                                                   genesis_account,
                                                   asset(2000),
                                                   asset(),
                                                   vector<char>()
                                                  }));
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );

      fee = trx.operations.front().get<transfer_operation>().fee;
      trx.validate();
      db.push_transaction(trx, ~0);

      BOOST_CHECK(genesis_account(db).balances(db).get_balance(asset_id_type()).amount == genesis_balance.amount + 2000);
      BOOST_CHECK(nathan_balances.get_balance(asset_id_type()) == asset(10000 - 2000 - fee.amount));

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_asset )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
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

      const asset_object& test_asset = test_asset_id(db);
      BOOST_CHECK(test_asset.symbol == "TEST");
      BOOST_CHECK(asset(1, test_asset_id) * test_asset.core_exchange_rate == asset(2));
      BOOST_CHECK(!test_asset.enforce_white_list());
      BOOST_CHECK(test_asset.max_supply == 100000);
      BOOST_CHECK(test_asset.short_backing_asset == asset_id_type());
      BOOST_CHECK(test_asset.market_fee_percent == 1);

      int test_num = 0;
      auto op = trx.operations.back().get<asset_create_operation>();
      ilog("Test duplicate symbol");
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test nonexistent issuer");
      CHECK_THROW_WITH_VALUE(op, issuer, account_id_type(99999999));
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test negative max supply");
      CHECK_THROW_WITH_VALUE(op, max_supply, -1);
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test zero max supply");
      CHECK_THROW_WITH_VALUE(op, max_supply, 0);
      ilog("Test single-letter symbol");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      CHECK_THROW_WITH_VALUE(op, symbol, "A");
      ilog("Test lower-case symbol");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      CHECK_THROW_WITH_VALUE(op, symbol, "qqq");
      ilog("Test two-digit symbol");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      CHECK_THROW_WITH_VALUE(op, symbol, "11");
      ilog("Test symbol with space");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      CHECK_THROW_WITH_VALUE(op, symbol, "AB CD");
      ilog("Test over-length symbol");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      CHECK_THROW_WITH_VALUE(op, symbol, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test negative core_exchange_rate");
      CHECK_THROW_WITH_VALUE(op, core_exchange_rate, price({asset(-100), asset(1)}));
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test negative core_exchange_rate (case 2)");
      CHECK_THROW_WITH_VALUE(op, core_exchange_rate, price({asset(100),asset(-1)}));
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test self-backing asset");
      CHECK_THROW_WITH_VALUE(op, short_backing_asset, db.get_index<asset_object>().get_next_id());
      trx.operations.back().get<asset_create_operation>().symbol = string("TEST") + char('A' + test_num++);
      ilog("Test nonexistent backing asset");
      CHECK_THROW_WITH_VALUE(op, short_backing_asset, asset_id_type(1000000));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_delegate )
{
   try {
      delegate_create_operation op;
      op.delegate_account = account_id_type();
      op.fee = asset();
      op.pay_rate = 0;
      op.first_secret_hash = secret_hash_type();
      op.signing_key = key_id_type();
      op.max_sec_until_expiration = op.block_interval_sec * 2;

      trx.operations.push_back(op);
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);

      for( int t = 0; t < FEE_TYPE_COUNT; ++t )
         op.fee_schedule.at(t) = BTS_BLOCKCHAIN_PRECISION;
      trx.operations.back() = op;

      CHECK_THROW_WITH_VALUE(op, fee_schedule.at(2), -500);
      CHECK_THROW_WITH_VALUE(op, delegate_account, account_id_type(99999999));
      CHECK_THROW_WITH_VALUE(op, fee, asset(-600));
      CHECK_THROW_WITH_VALUE(op, pay_rate, 123);
      CHECK_THROW_WITH_VALUE(op, signing_key, key_id_type(9999999));
      CHECK_THROW_WITH_VALUE(op, block_interval_sec, 0);
      CHECK_THROW_WITH_VALUE(op, max_block_size, 0);
      CHECK_THROW_WITH_VALUE(op, max_transaction_size, 0);
      CHECK_THROW_WITH_VALUE(op, max_sec_until_expiration, 0);
      trx.operations.back() = op;

      BOOST_CHECK(db.push_transaction(trx, ~0));
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
