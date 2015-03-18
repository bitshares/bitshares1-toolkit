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
      BOOST_CHECK(bitusd.symbol == "BITUSD");
      BOOST_CHECK(bitusd.short_backing_asset == asset_id_type());
      BOOST_CHECK(bitusd.dynamic_asset_data_id(db).current_supply == 0);
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
     // print_short_market("","");
      auto refund = cancel_short_order( *first_short );
      BOOST_REQUIRE( shorter_account.balances(db).get_balance( asset_id_type() ).amount == 10000-500 );
      FC_ASSERT( refund == asset(100) );
     // print_short_market("","");
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
      print_call_orders();
      //print_short_market("","");
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

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      //print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short  = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short  = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      //print_short_market("","");
      BOOST_REQUIRE( first_short && second_short && third_short );
      //print_joint_market("","");
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
      //print_short_market("","");
      print_call_orders();
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


BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new )
{ try {
   INVOKE( issue_uia );
   const asset_object&   core_asset     = get_asset( "TEST" );
   const asset_object&   test_asset     = get_asset( BTS_SYMBOL );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );
   const account_object& seller_account = create_account( "seller" );

   transfer( genesis_account(db), buyer_account, test_asset.amount( 10000 ) );
   transfer( nathan_account, seller_account, core_asset.amount(10000) );

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

   limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
   limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
   limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

   print_market( "", "" );
   auto unmatched = create_sell_order( seller_account, core_asset.amount(300), test_asset.amount(150) );
   print_market( "", "" );
   BOOST_CHECK( !db.find( first_id ) );
   BOOST_CHECK( !db.find( second_id ) );
   BOOST_CHECK( db.find( third_id ) );
   if( unmatched ) wdump((*unmatched));
   BOOST_CHECK( !unmatched );

   BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 200 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 297 );
   BOOST_CHECK_EQUAL( core_asset.dynamic_asset_data_id(db).accumulated_fees.value , 3 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( create_buy_exact_match_uia )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const asset_object&   core_asset     = get_asset( BTS_SYMBOL );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );
   const account_object& seller_account = create_account( "seller" );

   transfer( genesis_account(db), seller_account, asset( 10000 ) );
   transfer( nathan_account, buyer_account, test_asset.amount(10000) );

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

   limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
   limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
   limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

   print_market( "", "" );
   auto unmatched = create_sell_order( seller_account, core_asset.amount(100), test_asset.amount(100) );
   print_market( "", "" );
   BOOST_CHECK( !db.find( first_id ) );
   BOOST_CHECK( db.find( second_id ) );
   BOOST_CHECK( db.find( third_id ) );
   if( unmatched ) wdump((*unmatched));
   BOOST_CHECK( !unmatched );

   BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 99 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 100 );
   BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(db).accumulated_fees.value , 1 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}


BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new_reverse )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const asset_object&   core_asset     = get_asset( BTS_SYMBOL );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );
   const account_object& seller_account = create_account( "seller" );

   transfer( genesis_account(db), seller_account, asset( 10000 ) );
   transfer( nathan_account, buyer_account, test_asset.amount(10000) );

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );

   limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(100) )->id;
   limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(200) )->id;
   limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(300) )->id;

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

   print_market( "", "" );
   auto unmatched = create_sell_order( seller_account, core_asset.amount(300), test_asset.amount(150) );
   print_market( "", "" );
   BOOST_CHECK( !db.find( first_id ) );
   BOOST_CHECK( !db.find( second_id ) );
   BOOST_CHECK( db.find( third_id ) );
   if( unmatched ) wdump((*unmatched));
   BOOST_CHECK( !unmatched );

   BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 198 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 300 );
   BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(db).accumulated_fees.value , 2 );
 }
 catch ( const fc::exception& e )
 {
    elog( "${e}", ("e", e.to_detail_string() ) );
    throw;
 }
}

BOOST_AUTO_TEST_CASE( create_buy_uia_multiple_match_new_reverse_fract )
{ try {
   INVOKE( issue_uia );
   const asset_object&   test_asset     = get_asset( "TEST" );
   const asset_object&   core_asset     = get_asset( BTS_SYMBOL );
   const account_object& nathan_account = get_account( "nathan" );
   const account_object& buyer_account  = create_account( "buyer" );
   const account_object& seller_account = create_account( "seller" );

   transfer( genesis_account(db), seller_account, asset( 30 ) );
   transfer( nathan_account, buyer_account, test_asset.amount(10000) );

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 10000 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 0 );
   BOOST_CHECK_EQUAL( get_balance( seller_account, core_asset ), 30 );

   limit_order_id_type first_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(10) )->id;
   limit_order_id_type second_id = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(20) )->id;
   limit_order_id_type third_id  = create_sell_order( buyer_account, test_asset.amount(100), core_asset.amount(30) )->id;

   BOOST_CHECK_EQUAL( get_balance( buyer_account, test_asset ), 9700 );

   print_market( "", "" );
   auto unmatched = create_sell_order( seller_account, core_asset.amount(30), test_asset.amount(150) );
   print_market( "", "" );
   BOOST_CHECK( !db.find( first_id ) );
   BOOST_CHECK( !db.find( second_id ) );
   BOOST_CHECK( db.find( third_id ) );
   if( unmatched ) wdump((*unmatched));
   BOOST_CHECK( !unmatched );

   BOOST_CHECK_EQUAL( get_balance( seller_account, test_asset ), 198 );
   BOOST_CHECK_EQUAL( get_balance( buyer_account, core_asset ), 30 );
   BOOST_CHECK_EQUAL( get_balance( seller_account, core_asset ), 0 );
   BOOST_CHECK_EQUAL( test_asset.dynamic_asset_data_id(db).accumulated_fees.value , 2 );
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

BOOST_AUTO_TEST_CASE( delegate_feeds )
{
   try {
      INVOKE( create_mia );
      const asset_object& bit_usd = get_asset("BITUSD");
      const vector<delegate_id_type>& active_delegates = db.get_global_properties().active_delegates;
      BOOST_REQUIRE(active_delegates.size() == 10);

      delegate_publish_feeds_operation op({active_delegates[0], asset()});
      op.feeds.insert(price_feed());
      // Supposing 1 core asset is worth 2 cents... Don't call if it hasn't reached 1 cent
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(1));
      // Block shorts at more than 3 cents
      op.feeds.begin()->short_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(3));
      // We'll expire margins after a month
      op.feeds.begin()->max_margin_period_sec = fc::days(30).to_seconds();
      // Accept defaults for required collateral
      trx.operations.emplace_back(op);
      db.push_transaction(trx, ~0);

      BOOST_CHECK(bit_usd.current_feed.call_limit.to_real() == BTS_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bit_usd.current_feed.short_limit.to_real() == BTS_BLOCKCHAIN_PRECISION / 3.0);
      BOOST_CHECK(bit_usd.current_feed.max_margin_period_sec == fc::days(30).to_seconds());
      BOOST_CHECK(bit_usd.current_feed.required_initial_collateral == BTS_DEFAULT_INITIAL_COLLATERAL_RATIO);
      BOOST_CHECK(bit_usd.current_feed.required_maintenance_collateral == BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.delegate = active_delegates[1];
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(2));
      op.feeds.begin()->short_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(2));
      op.feeds.begin()->max_margin_period_sec = fc::days(10).to_seconds();
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(bit_usd.current_feed.call_limit.to_real() == BTS_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bit_usd.current_feed.short_limit.to_real() == BTS_BLOCKCHAIN_PRECISION / 2.0);
      BOOST_CHECK(bit_usd.current_feed.max_margin_period_sec == fc::days(30).to_seconds());
      BOOST_CHECK(bit_usd.current_feed.required_initial_collateral == BTS_DEFAULT_INITIAL_COLLATERAL_RATIO);
      BOOST_CHECK(bit_usd.current_feed.required_maintenance_collateral == BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.delegate = active_delegates[2];
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(1));
      op.feeds.begin()->short_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(4));
      op.feeds.begin()->max_margin_period_sec = fc::days(100).to_seconds();
      // But this delegate is an idiot.
      op.feeds.begin()->required_initial_collateral = 1001;
      op.feeds.begin()->required_maintenance_collateral = 1000;
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(bit_usd.current_feed.call_limit.to_real() == BTS_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bit_usd.current_feed.short_limit.to_real() == BTS_BLOCKCHAIN_PRECISION / 3.0);
      BOOST_CHECK(bit_usd.current_feed.max_margin_period_sec == fc::days(30).to_seconds());
      BOOST_CHECK(bit_usd.current_feed.required_initial_collateral == BTS_DEFAULT_INITIAL_COLLATERAL_RATIO);
      BOOST_CHECK(bit_usd.current_feed.required_maintenance_collateral == BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);
   } catch (const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  Assume there exists an offer to buy BITUSD
 *  Create a short that exactly matches that offer at a price 2:1
 */
BOOST_AUTO_TEST_CASE( limit_match_existing_short_exact )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      //print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      //print_short_market("","");
      BOOST_REQUIRE( first_short && second_short && third_short );
      //print_joint_market("","");
      auto unmatched_order = create_sell_order( buyer_account, asset(200), bitusd.amount(100) );
      //print_joint_market("","");
      BOOST_REQUIRE( !unmatched_order );
      // now it shouldn't fill
      unmatched_order = create_sell_order( buyer_account, asset(200), bitusd.amount(100) );
      //print_joint_market("","");
      BOOST_REQUIRE( unmatched_order );
      BOOST_CHECK( unmatched_order->amount_for_sale() == asset(200) );
      BOOST_CHECK( unmatched_order->amount_to_receive() == bitusd.amount(100) );
      BOOST_CHECK( second_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( third_short->amount_for_sale() == bitusd.amount(100) );
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
BOOST_AUTO_TEST_CASE( limit_match_existing_short_partial_exact_price )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      //print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      //print_short_market("","");
      BOOST_REQUIRE( first_short && second_short && third_short );
      //print_joint_market("","");
      auto unmatched_order = create_sell_order( buyer_account, asset(100), bitusd.amount(50) );
      //print_joint_market("","");
      BOOST_REQUIRE( !unmatched_order );
      BOOST_CHECK( first_short->amount_for_sale() == bitusd.amount(50) );
      BOOST_CHECK( first_short->get_collateral()  == asset(100) );
      BOOST_CHECK( second_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( third_short->amount_for_sale() == bitusd.amount(100) );

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
BOOST_AUTO_TEST_CASE( limit_match_existing_short_partial_over_price )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      //print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      BOOST_REQUIRE( first_short && second_short && third_short );
      //print_joint_market("","");
      auto unmatched_order = create_sell_order( buyer_account, asset(100), bitusd.amount(40) );
      //print_joint_market("","");
      BOOST_REQUIRE( !unmatched_order );
      BOOST_CHECK( first_short->amount_for_sale() == bitusd.amount(50) );
      BOOST_CHECK( first_short->get_collateral()  == asset(100) );
      BOOST_CHECK( second_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( third_short->amount_for_sale() == bitusd.amount(100) );

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
BOOST_AUTO_TEST_CASE( limit_match_multiple_existing_short_partial_over_price )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      //print_market("","");
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto next_short = create_short( shorter_account, bitusd.amount(100), asset( 210 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      //print_short_market("","");
      BOOST_REQUIRE( first_short && second_short && third_short );
      auto unmatched_order = create_sell_order( buyer_account, asset(200+115), bitusd.amount(150) );
     // print_joint_market("","");
      BOOST_REQUIRE( !unmatched_order );
      //wdump( (next_short->amount_for_sale().amount)(next_short->get_collateral().amount) );
      BOOST_CHECK( next_short->amount_for_sale() == bitusd.amount(46) );
      BOOST_CHECK( next_short->get_collateral()  == asset(97) );
      BOOST_CHECK( second_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( third_short->amount_for_sale() == bitusd.amount(100) );
      print_call_orders();

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
BOOST_AUTO_TEST_CASE( limit_dont_match_existing_short_partial_over_price )
{
   try {
      const asset_object& bitusd = create_bitasset( "BITUSD" );
      const account_object& shorter_account  = create_account( "shorter" );
      const account_object& buyer_account  = create_account( "buyer" );
      transfer( genesis_account(db), shorter_account, asset( 10000 ) );
      transfer( genesis_account(db), buyer_account, asset( 10000 ) );

      create_sell_order( buyer_account, asset(125), bitusd.amount(100) );
      create_sell_order( buyer_account, asset(150), bitusd.amount(100) );
      auto buy_order = create_sell_order( buyer_account, asset(100), bitusd.amount(100) );
      BOOST_REQUIRE( buy_order );
      auto first_short = create_short( shorter_account, bitusd.amount(100), asset( 200 ) ); // 1:1 price
      auto second_short = create_short( shorter_account, bitusd.amount(100), asset( 300 ) ); // 1:1 price
      auto third_short = create_short( shorter_account, bitusd.amount(100), asset( 400 ) ); // 1:1 price
      //print_short_market("","");
      BOOST_REQUIRE( first_short && second_short && third_short );
      //print_joint_market("","");
      auto unmatched_order = create_sell_order( buyer_account, asset(100), bitusd.amount(60) );
      BOOST_REQUIRE( unmatched_order );
      BOOST_CHECK( first_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( first_short->get_collateral()  == asset(200) );
      BOOST_CHECK( second_short->amount_for_sale() == bitusd.amount(100) );
      BOOST_CHECK( third_short->amount_for_sale() == bitusd.amount(100) );
   }catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( multiple_shorts_matching_multiple_bids_in_order )
{ try {
   const asset_object& bitusd = create_bitasset( "BITUSD" );
   const account_object& shorter1_account  = create_account( "shorter1" );
   const account_object& shorter2_account  = create_account( "shorter2" );
   const account_object& shorter3_account  = create_account( "shorter3" );
   const account_object& buyer_account  = create_account( "buyer" );
   transfer( genesis_account(db), shorter1_account, asset( 10000 ) );
   transfer( genesis_account(db), shorter2_account, asset( 10000 ) );
   transfer( genesis_account(db), shorter3_account, asset( 10000 ) );
   transfer( genesis_account(db), buyer_account, asset( 10000 ) );

   BOOST_REQUIRE( create_sell_order( buyer_account, asset(125), bitusd.amount(100) ) );
   BOOST_REQUIRE( create_sell_order( buyer_account, asset(150), bitusd.amount(100) ) );
   BOOST_REQUIRE( create_sell_order( buyer_account, asset(200), bitusd.amount(100) ) );
   print_joint_market("","");
   BOOST_REQUIRE( !create_short( shorter1_account, bitusd.amount(100), asset( 200 ) ) );
   BOOST_REQUIRE( !create_short( shorter2_account, bitusd.amount(100), asset( 150 ) ) );
   BOOST_REQUIRE( !create_short( shorter3_account, bitusd.amount(100), asset( 125 ) ) );
   print_call_orders();

}catch ( const fc::exception& e )
{
  elog( "${e}", ("e", e.to_detail_string() ) );
  throw;
} }

BOOST_AUTO_TEST_CASE( limit_order_matching_mix_of_shorts_and_limits )
{ try {
   const asset_object& bitusd      = create_bitasset( "BITUSD" );
   const asset_object& bts         = get_asset( BTS_SYMBOL );
   const account_object& shorter1  = create_account( "shorter1" );
   const account_object& shorter2  = create_account( "shorter2" );
   const account_object& shorter3  = create_account( "shorter3" );
   const account_object& buyer1    = create_account( "buyer1" );
   const account_object& buyer2    = create_account( "buyer2" );
   const account_object& buyer3    = create_account( "buyer3" );

   wlog(".");
   transfer( genesis_account(db), shorter1, asset( 10000 ) );
   transfer( genesis_account(db), shorter2, asset( 10000 ) );
   transfer( genesis_account(db), shorter3, asset( 10000 ) );
   wlog(".");
   transfer( genesis_account(db), buyer1, asset( 10000 ) );
   wlog(".");
   transfer( genesis_account(db), buyer2, asset( 10000 ) );
   transfer( genesis_account(db), buyer3, asset( 10000 ) );
   wlog(".");

   // create some BitUSD
   BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
   BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
   BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

   wlog(".");

   // create a mixture of BitUSD sells and shorts
   BOOST_REQUIRE( create_short(      shorter1, bitusd.amount(100), asset(125) )   );
   BOOST_REQUIRE( create_sell_order( buyer1,   bitusd.amount(100), asset(150) )   );
   BOOST_REQUIRE( create_short(      shorter2, bitusd.amount(100), asset(200) )   );
   BOOST_REQUIRE( create_sell_order( buyer1,   bitusd.amount(100), asset(225) )   );
   BOOST_REQUIRE( create_short(      shorter3, bitusd.amount(100), asset(250) )   );
   
   print_joint_market("",""); // may have bugs

   // buy up everything but the highest order
   auto unfilled_order = create_sell_order( buyer2, asset(700), bitusd.amount(311) );
   if( unfilled_order ) wdump((*unfilled_order));
   //wdump( (get_balance(buyer2,bts)) );
   //wdump( (get_balance(buyer2,bitusd)) );
   print_joint_market("","");
   if( unfilled_order ) wdump((*unfilled_order));
   BOOST_REQUIRE( !unfilled_order );
   BOOST_REQUIRE_EQUAL( get_balance(buyer2, bitusd), 396 );

   print_joint_market("","");
   print_call_orders();

}catch ( const fc::exception& e )
{
  elog( "${e}", ("e", e.to_detail_string() ) );
  throw;
} }


BOOST_AUTO_TEST_SUITE_END()
