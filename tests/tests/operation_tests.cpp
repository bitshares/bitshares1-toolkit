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

BOOST_AUTO_TEST_CASE( create_account_test )
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
      INVOKE(create_account_test);

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
      db.push_transaction(trx, ~0);
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
      db.push_transaction(trx, ~0);

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

BOOST_AUTO_TEST_CASE( create_mia )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      (void)bitusd;
      BOOST_REQUIRE_THROW( create_bitasset("BITUSD"), fc::exception);
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_short_test )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 100 ) ); // 1:1 price
      BOOST_REQUIRE( first_short != nullptr );
      BOOST_REQUIRE( create_short( shorter_account, bitusd.amount(100), asset( 200 ) ) ); // 1:2 price
      BOOST_REQUIRE( create_short( shorter_account, bitusd.amount(100), asset( 300 ) ) ); // 1:3 price
      BOOST_REQUIRE( shorter_account.balances(db).get_balance( asset_id_type() ).amount == 10000-600 ); 
      print_short_market("","");
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}
BOOST_AUTO_TEST_CASE( cancel_short_test )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 100 ) ); // 1:1 price
      BOOST_REQUIRE( first_short != nullptr );
      BOOST_REQUIRE( create_short( shorter_account, bitusd.amount(100), asset( 200 ) ) ); // 1:2 price
      BOOST_REQUIRE( create_short( shorter_account, bitusd.amount(100), asset( 300 ) ) ); // 1:3 price
      BOOST_REQUIRE( shorter_account.balances(db).get_balance( asset_id_type() ).amount == 10000-600 ); 
      print_short_market("","");
      auto refund = cancel_short_order( *first_short );
      BOOST_REQUIRE( shorter_account.balances(db).get_balance( asset_id_type() ).amount == 10000-500 ); 
      FC_ASSERT( refund == asset(100) );
      print_short_market("","");
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

/**
 *  Assume there exists an offer to buy BITUSD
 *  Create a short that exactly matches that offer at a price 2:1
 */
BOOST_AUTO_TEST_CASE( match_short_now_exact )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      auto buy_order = create_sell_order( buyer_account, asset(200), bitusd.amount(100) );
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      BOOST_REQUIRE( first_short == nullptr );
      print_short_market("","");
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

/**
 *  Assume there exists an offer to buy BITUSD
 *  Create a short that exactly matches that offer at a price 2:1
 */
BOOST_AUTO_TEST_CASE( dont_match_short )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      //auto buy_order = create_sell_order( buyer_account, asset(200), bitusd.amount(101) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      print_short_market("","");
      BOOST_REQUIRE( first_short );
      print_short_market("","");
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}
/**
 *  Assume there exists an offer to buy BITUSD
 *  Create a short that exactly matches that offer at a price 2:1
 */
BOOST_AUTO_TEST_CASE( match_all_short_with_surplus_collaterl )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      //auto buy_order = create_sell_order( buyer_account, asset(200), bitusd.amount(101) );
      auto buy_order = create_sell_order( buyer_account, asset(300), bitusd.amount(100) );
      print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      print_short_market("","");
      BOOST_REQUIRE( !first_short );
      print_short_market("","");
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
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
      creator.max_supply = 100000000;
      creator.precision = 2;
      creator.market_fee_percent = BTS_MAX_MARKET_FEE_PERCENT/100; /*1%*/
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
      BOOST_CHECK(test_asset.max_supply == 100000000);
      BOOST_CHECK(test_asset.short_backing_asset == asset_id_type());
      BOOST_CHECK(test_asset.market_fee_percent == BTS_MAX_MARKET_FEE_PERCENT/100);
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);

      auto op = trx.operations.back().get<asset_create_operation>();
      op.symbol = "TESTFAIL";
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
      INVOKE(create_account_test);

      const asset_object& test_asset = *db.get_index_type<asset_index>().indices().get<by_symbol>().find("TEST");
      const account_object& nathan_account = *db.get_index_type<account_index>().indices().get<by_name>().find("nathan");

      asset_issue_operation op({test_asset.amount(5000000), asset(), nathan_account.id});
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, asset_to_issue, asset(200));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, issue_to_account, account_id_type(999999999));

      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      const asset_dynamic_data_object& test_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == test_asset.amount(5000000));
      BOOST_CHECK(test_dynamic_data.current_supply == 5000000);
      BOOST_CHECK(test_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_dynamic_data.fee_pool == 0);

      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == test_asset.amount(10000000));
      BOOST_CHECK(test_dynamic_data.current_supply == 10000000);
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

      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(10000000));
      trx.operations.push_back(transfer_operation({nathan.id, genesis.id, uia.amount(5000)}));
      db.push_transaction(trx, ~0);
      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(10000000 - 5000));
      BOOST_CHECK(genesis.balances(db).get_balance(uia.id) == uia.amount(5000));

      db.push_transaction(trx, ~0);
      BOOST_CHECK(nathan.balances(db).get_balance(uia.id) == uia.amount(10000000 - 10000));
      BOOST_CHECK(genesis.balances(db).get_balance(uia.id) == uia.amount(10000));
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( create_buy_uia_exact_match )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );

   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   for( uint32_t i = 0; i < 3; ++i )
      create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*i) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == (asset( 7000 )) );

   for( uint32_t i = 0; i < 3; ++i )
      create_sell_order( nathan_account, test_asset.amount(1000), asset(100+450*i) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(test_asset.id) == test_asset.amount(990) );
   BOOST_CHECK( nathan_account.balances(db).get_balance(asset_id_type()) == asset(1000) );
   BOOST_CHECK( test_asset.dynamic_asset_data_id(db).accumulated_fees.value == 10 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( create_buy_uia_partial_match_new )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );

   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   for( uint32_t i = 0; i < 3; ++i )
      create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*i) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == (asset( 7000 )) );

   for( uint32_t i = 0; i < 3; ++i )
      BOOST_CHECK(create_sell_order( nathan_account, test_asset.amount(1000*1.1), asset((100+450*i)*1.1) ));

   print_market( "", "" );

   BOOST_CHECK( buyer_account.balances(db).get_balance(test_asset.id) == test_asset.amount(990) );
   BOOST_CHECK( nathan_account.balances(db).get_balance(asset_id_type()) == asset(1000) );
   BOOST_CHECK( test_asset.dynamic_asset_data_id(db).accumulated_fees.value == 10 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );

   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   for( uint32_t i = 0; i < 3; ++i )
      create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*i) );
   create_sell_order( buyer_account, asset(500), test_asset.amount(100+450) );
   create_sell_order( buyer_account, asset(500), test_asset.amount(100+450) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == (asset( 6000 )) );

   for( uint32_t i = 0; i < 2; ++i )
      BOOST_CHECK(create_sell_order( nathan_account, test_asset.amount(1000*1.1), asset((100+450*i)*1.1) ));
   BOOST_CHECK(nullptr == create_sell_order( nathan_account, test_asset.amount(1000*1.1), asset((100+450*2)*1.1) ));

   print_market( "", "" );

   BOOST_CHECK( buyer_account.balances(db).get_balance(test_asset.id) == test_asset.amount(1090) );
   BOOST_CHECK( nathan_account.balances(db).get_balance(asset_id_type()) == asset(1000) );

   /** NOTE: the fee is 10 despite 1% of 1100 being 11 because to orders of size
    *   550 which result in fees of 5.5 and 5.5 respectively have the .5 truncated
    *   resulting in 5+5 == 10.
    */
   BOOST_CHECK( test_asset.dynamic_asset_data_id(db).accumulated_fees.value == 10 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}


BOOST_AUTO_TEST_CASE( create_buy_uia_partial_match_prior )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );

   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   for( uint32_t i = 0; i < 3; ++i )
      create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*i) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == (asset( 7000 )) );

   for( uint32_t i = 0; i < 2; ++i )
      BOOST_CHECK(create_sell_order( nathan_account, test_asset.amount(1000*.9), asset((100+450*i)*.9) ));
   BOOST_CHECK(! create_sell_order( nathan_account, test_asset.amount(1000*.9), asset((100+450*2)*.9) ));

   print_market( "", "" );

   wdump( (buyer_account.balances(db).get_balance(test_asset.id) ) );
   wdump( (nathan_account.balances(db).get_balance(test_asset.id) ) );
   wdump( (nathan_account.balances(db).get_balance(asset_id_type()) ) );
   wdump( (test_asset.dynamic_asset_data_id(db).accumulated_fees.value) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(test_asset.id) == test_asset.amount(891) );
   BOOST_CHECK( nathan_account.balances(db).get_balance(asset_id_type()) == asset(900) );
   BOOST_CHECK( test_asset.dynamic_asset_data_id(db).accumulated_fees.value == 9 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( uia_fees )
{
   try {
      INVOKE( issue_uia );

      enable_fees();

      const asset_object& test_asset = get_asset("TEST");
      const asset_dynamic_data_object& asset_dynamic = test_asset.dynamic_asset_data_id(db);
      const account_object& nathan_account = get_account("nathan");
      const account_object& genesis_account = account_id_type()(db);

      fund_fee_pool(genesis_account, test_asset, 1000000);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000000);

      transfer_operation op({nathan_account.id, genesis_account.id, test_asset.amount(100)});
      op.fee = asset(op.calculate_fee(db.current_fee_schedule())) * test_asset.core_exchange_rate;
      BOOST_CHECK(op.fee.asset_id == test_asset.id);
      asset old_balance = nathan_account.balances(db).get_balance(test_asset.id);
      asset fee = op.fee;
      BOOST_CHECK(fee.amount > 0);
      asset core_fee = fee*test_asset.core_exchange_rate;
      trx.operations.push_back(std::move(op));
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == old_balance - fee - test_asset.amount(100));
      BOOST_CHECK(genesis_account.balances(db).get_balance(test_asset.id) == test_asset.amount(100));
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000000 - core_fee.amount);

      //Do it again, for good measure.
      db.push_transaction(trx, ~0);
      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id)
                  == old_balance - fee - fee - test_asset.amount(200));
      BOOST_CHECK(genesis_account.balances(db).get_balance(test_asset.id) == test_asset.amount(200));
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount + fee.amount);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000000 - core_fee.amount - core_fee.amount);

      op = std::move(trx.operations.back().get<transfer_operation>());
      trx.operations.clear();
      op.amount = asset(20);

      asset genesis_balance_before = genesis_account.balances(db).get_balance(asset_id_type());
      BOOST_CHECK(nathan_account.balances(db).get_balance(asset_id_type()) == asset());
      transfer(genesis_account, nathan_account, asset(20));
      BOOST_CHECK(nathan_account.balances(db).get_balance(asset_id_type()) == asset(20));

      trx.operations.emplace_back(std::move(op));
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan_account.balances(db).get_balance(asset_id_type()) == asset());
      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id)
                  == old_balance - fee - fee - fee - test_asset.amount(200));
      BOOST_CHECK(genesis_account.balances(db).get_balance(test_asset.id) == test_asset.amount(200));
      BOOST_CHECK(genesis_account.balances(db).get_balance(asset_id_type())
                  == genesis_balance_before - asset(BTS_BLOCKCHAIN_PRECISION));
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount.value * 3);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000000 - core_fee.amount.value * 3);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( cancel_limit_order_test )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const account_object& buyer_account  = create_account( "buyer" );

   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset( 10000 ) );
   auto sell_order = create_sell_order( buyer_account, asset(1000), test_asset.amount(100+450*1) );
   FC_ASSERT( sell_order );
   auto refunded = cancel_limit_order( *sell_order );
   BOOST_CHECK( refunded == asset(1000) );
   BOOST_CHECK( buyer_account.balances(db).get_balance(asset_id_type()) == asset(10000) );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}


BOOST_AUTO_TEST_SUITE_END()
