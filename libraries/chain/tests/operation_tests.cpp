#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/simple_index.hpp>

#include <fc/crypto/digest.hpp>

#include "database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

///Shortcut to require an exception when processing a transaction with an operation containing an expected bad value
/// Uses require insteach of check, because these transactions are expected to fail. If they don't, subsequent tests
/// may spuriously succeed or fail due to unexpected database state.
#define REQUIRE_THROW_WITH_VALUE(op, field, value) \
{ \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception); \
}
///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((test*)this)->test_method(); RESET(trx); trx.relative_expiration = 1000

BOOST_AUTO_TEST_CASE( create_account )
{
   try {
      trx.operations.push_back(make_account());
      account_create_operation op = trx.operations.back().get<account_create_operation>();

      REQUIRE_THROW_WITH_VALUE(op, fee_paying_account, account_id_type(9999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, name, "!");
      REQUIRE_THROW_WITH_VALUE(op, name, "Sam");
      REQUIRE_THROW_WITH_VALUE(op, name, "saM");
      REQUIRE_THROW_WITH_VALUE(op, name, "sAm");
      REQUIRE_THROW_WITH_VALUE(op, name, "6j");
      REQUIRE_THROW_WITH_VALUE(op, name, "j-");
      REQUIRE_THROW_WITH_VALUE(op, name, "-j");
      REQUIRE_THROW_WITH_VALUE(op, name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
      REQUIRE_THROW_WITH_VALUE(op, name, "a.");
      REQUIRE_THROW_WITH_VALUE(op, name, ".a");
      REQUIRE_THROW_WITH_VALUE(op, voting_key, key_id_type(999999999));
      REQUIRE_THROW_WITH_VALUE(op, memo_key, key_id_type(999999999));

      auto auth_bak = op.owner;
      op.owner.add_authority(account_id_type(9999999999), 10);
      trx.operations.back() = op;
      op.owner = auth_bak;
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      op.owner = auth_bak;
      op.owner.add_authority(key_id_type(9999999999), 10);
      trx.operations.back() = op;
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      op.owner = auth_bak;

      trx.operations.back() = op;
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

BOOST_AUTO_TEST_CASE( transfer_core_asset )
{
   try {
      INVOKE(create_account);

      account_id_type genesis_account;
      asset genesis_balance = genesis_account(db).balances(db).get_balance(asset_id_type());

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

BOOST_AUTO_TEST_CASE( create_delegate )
{
   try {
      delegate_create_operation op;
      op.delegate_account = account_id_type();
      op.fee = asset();
      op.pay_rate = 50;
      op.first_secret_hash = secret_hash_type::hash("my 53cr37 p4s5w0rd");
      op.signing_key = key_id_type();
      op.block_interval_sec = BTS_DEFAULT_BLOCK_INTERVAL + 1;
      op.max_block_size = BTS_DEFAULT_MAX_BLOCK_SIZE + 1;
      op.max_transaction_size = BTS_DEFAULT_MAX_TRANSACTION_SIZE + 1;
      op.max_sec_until_expiration = op.block_interval_sec * 2;

      for( int t = 0; t < FEE_TYPE_COUNT; ++t )
         op.fee_schedule.at(t) = 0;
      trx.operations.push_back(op);
      //Zero fee schedule should cause failure
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);

      for( int t = 0; t < FEE_TYPE_COUNT; ++t )
         op.fee_schedule.at(t) = BTS_BLOCKCHAIN_PRECISION;
      trx.operations.back() = op;

      REQUIRE_THROW_WITH_VALUE(op, fee_schedule.at(2), -500);
      REQUIRE_THROW_WITH_VALUE(op, delegate_account, account_id_type(99999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-600));
      REQUIRE_THROW_WITH_VALUE(op, pay_rate, 123);
      REQUIRE_THROW_WITH_VALUE(op, signing_key, key_id_type(9999999));
      REQUIRE_THROW_WITH_VALUE(op, block_interval_sec, 0);
      REQUIRE_THROW_WITH_VALUE(op, max_block_size, 0);
      REQUIRE_THROW_WITH_VALUE(op, max_transaction_size, 0);
      REQUIRE_THROW_WITH_VALUE(op, max_sec_until_expiration, 0);
      trx.operations.back() = op;

      delegate_id_type delegate_id = db.get_index_type<primary_index<simple_index<delegate_object>>>().get_next_id();
      BOOST_CHECK(db.push_transaction(trx, ~0));
      const delegate_object& d = delegate_id(db);

      BOOST_CHECK(d.delegate_account == account_id_type());
      BOOST_CHECK(d.signing_key == key_id_type());
      BOOST_CHECK(d.pay_rate == 50);
      BOOST_CHECK(d.block_interval_sec == BTS_DEFAULT_BLOCK_INTERVAL + 1);
      BOOST_CHECK(d.max_block_size == BTS_DEFAULT_MAX_BLOCK_SIZE + 1);
      BOOST_CHECK(d.max_transaction_size == BTS_DEFAULT_MAX_TRANSACTION_SIZE + 1);
      BOOST_CHECK(d.max_sec_until_expiration == d.block_interval_sec * 2);
      BOOST_CHECK(d.next_secret == secret_hash_type::hash("my 53cr37 p4s5w0rd"));
      BOOST_CHECK(d.last_secret == secret_hash_type());
      BOOST_CHECK(d.accumulated_income == 0);
      BOOST_CHECK(d.vote(db).total_votes == 0);

      for( int i = 0; i < FEE_TYPE_COUNT; ++i )
         BOOST_CHECK(d.fee_schedule.at(i) == BTS_BLOCKCHAIN_PRECISION);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_delegate )
{
   try {
      INVOKE(create_delegate);

      delegate_id_type delegate_id = db.get_index_type<primary_index<simple_index<delegate_object>>>().get_next_id().instance() - 1;
      const delegate_object& d = delegate_id(db);
      BOOST_CHECK(d.next_secret == secret_hash_type::hash("my 53cr37 p4s5w0rd"));

      delegate_update_operation op;
      trx.operations.push_back(op);
      op.delegate_id = delegate_id;
      op.fee = asset();
      op.pay_rate = 100;
      op.block_interval_sec = d.block_interval_sec / 2;
      op.max_block_size = d.max_block_size / 2;
      op.max_transaction_size = d.max_transaction_size / 2;
      op.max_sec_until_expiration = d.max_sec_until_expiration / 2;

      op.fee_schedule = decltype(d.fee_schedule)();
      for( int t = 0; t < FEE_TYPE_COUNT; ++t )
         op.fee_schedule->at(t) = BTS_BLOCKCHAIN_PRECISION / 2;

      REQUIRE_THROW_WITH_VALUE(op, delegate_id, delegate_id_type(9999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-5));
      REQUIRE_THROW_WITH_VALUE(op, signing_key, object_id_type(protocol_ids, key_object_type, 999999999));
      REQUIRE_THROW_WITH_VALUE(op, pay_rate, 127);
      REQUIRE_THROW_WITH_VALUE(op, block_interval_sec, 0);
      REQUIRE_THROW_WITH_VALUE(op, block_interval_sec, BTS_MAX_BLOCK_INTERVAL + 1);
      REQUIRE_THROW_WITH_VALUE(op, max_block_size, 0);
      REQUIRE_THROW_WITH_VALUE(op, max_transaction_size, 0);
      REQUIRE_THROW_WITH_VALUE(op, max_sec_until_expiration, op.block_interval_sec - 1);
      REQUIRE_THROW_WITH_VALUE(op, fee_schedule->at(0), 0);

      trx.operations.back() = op;
      BOOST_CHECK(db.push_transaction(trx, ~0));

      BOOST_CHECK(d.delegate_account == account_id_type());
      BOOST_CHECK(d.pay_rate == 100);
      BOOST_CHECK(d.block_interval_sec == (BTS_DEFAULT_BLOCK_INTERVAL + 1) / 2);
      BOOST_CHECK(d.max_block_size == (BTS_DEFAULT_MAX_BLOCK_SIZE + 1) / 2);
      BOOST_CHECK(d.max_transaction_size == (BTS_DEFAULT_MAX_TRANSACTION_SIZE + 1) / 2);
      BOOST_CHECK(d.max_sec_until_expiration == d.block_interval_sec * 2);
      BOOST_CHECK(d.next_secret == secret_hash_type::hash("my 53cr37 p4s5w0rd"));
      BOOST_CHECK(d.last_secret == secret_hash_type());
      BOOST_CHECK(d.accumulated_income == 0);
      BOOST_CHECK(d.vote(db).total_votes == 0);

      for( int i = 0; i < FEE_TYPE_COUNT; ++i )
         BOOST_CHECK(d.fee_schedule.at(i) == BTS_BLOCKCHAIN_PRECISION / 2);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_uia )
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

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);

      auto op = trx.operations.back().get<asset_create_operation>();
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      REQUIRE_THROW_WITH_VALUE(op, issuer, account_id_type(99999999));
      REQUIRE_THROW_WITH_VALUE(op, max_supply, -1);
      REQUIRE_THROW_WITH_VALUE(op, max_supply, 0);
      REQUIRE_THROW_WITH_VALUE(op, symbol, "A");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "qqq");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "11");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "AB CD");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, price({asset(-100), asset(1)}));
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, price({asset(100),asset(-1)}));
      REQUIRE_THROW_WITH_VALUE(op, short_backing_asset, db.get_index<asset_object>().get_next_id());
      REQUIRE_THROW_WITH_VALUE(op, short_backing_asset, asset_id_type(1000000));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_uia )
{
   try {
      INVOKE(create_uia);
      INVOKE(create_account);

      const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");
      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");

      asset_issue_operation op({test_asset.amount(5000), asset(), nathan_account.id});
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, asset_to_issue, asset(200));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, issue_to_account, account_id_type(999999999));

      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      const asset_dynamic_data_object& test_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == test_asset.amount(5000));
      BOOST_CHECK(test_dynamic_data.current_supply == 5000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_dynamic_data.fee_pool == 0);

      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == test_asset.amount(10000));
      BOOST_CHECK(test_dynamic_data.current_supply == 10000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_dynamic_data.fee_pool == 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_uia )
{
   try {
      INVOKE(issue_uia);

      const asset_object& uia = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");
      const account_object& nathan = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");
      const account_object& genesis = account_id_type()(db);

      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(10000));
      trx.operations.push_back(transfer_operation({nathan.id, genesis.id, uia.amount(5000)}));
      db.push_transaction(trx, ~0);
      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(5000));
      BOOST_CHECK(genesis.balances(db).get_balance(uia.id) == uia.amount(5000));

      db.push_transaction(trx, ~0);
      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(0));
      BOOST_CHECK(genesis.balances(db).get_balance(uia.id) == uia.amount(10000));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_buy_uia_order )
{ try {
   INVOKE( issue_uia );
   const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");
   const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");

   trx.operations.push_back(make_account("buyer"));
   trx.operations.push_back(make_account("seller"));
   trx.signatures.push_back(fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis"))).sign_compact(fc::digest(trx)));
   trx.validate();
   db.push_transaction(trx, ~0);
   trx.operations.clear();

   const account_object& buyer_account = *db.get_index_type<account_index>().indices().get<by_name>().find("buyer");
   const account_object& seller_account = *db.get_index_type<account_index>().indices().get<by_name>().find("seller");

   account_id_type genesis_account;
   asset genesis_balance = genesis_account(db).balances(db).get_balance(asset_id_type());

   trx.operations.push_back(transfer_operation({genesis_account,
                                                buyer_account.id,
                                                asset(10000),
                                                asset(),
                                                vector<char>()
                                               }));
   for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );

   trx.validate();
   db.push_transaction(trx, ~0);
   trx.operations.clear();

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   limit_order_create_operation buy_order;
   buy_order.seller = buyer_account.id;
   buy_order.amount_to_sell = asset( 5000 );
   buy_order.min_to_receive = asset( 300, test_asset.id );
   trx.operations.push_back(buy_order);
   for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
   trx.validate();
   auto fee = trx.operations.back().get<limit_order_create_operation>().fee;
   db.push_transaction(trx, ~0);
   trx.operations.clear();
   wdump((fee));
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == (asset( 5000 )-fee) );


 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
 }

}



BOOST_AUTO_TEST_SUITE_END()
