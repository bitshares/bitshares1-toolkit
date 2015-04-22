#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( operation_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_account_test )
{
   try {
      trx.operations.push_back(make_account());
      account_create_operation op = trx.operations.back().get<account_create_operation>();

      REQUIRE_THROW_WITH_VALUE(op, registrar, account_id_type(9999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-1));
      REQUIRE_THROW_WITH_VALUE(op, name, "!");
      REQUIRE_THROW_WITH_VALUE(op, name, "Sam");
      REQUIRE_THROW_WITH_VALUE(op, name, "saM");
      REQUIRE_THROW_WITH_VALUE(op, name, "sAm");
      REQUIRE_THROW_WITH_VALUE(op, name, "6j");
      REQUIRE_THROW_WITH_VALUE(op, name, "j-");
      REQUIRE_THROW_WITH_VALUE(op, name, "-j");
      REQUIRE_THROW_WITH_VALUE(op, name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
      REQUIRE_THROW_WITH_VALUE(op, name, "aaaa.");
      REQUIRE_THROW_WITH_VALUE(op, name, ".aaaa");
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
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( child_account )
{
   try {
      INVOKE(create_account_test);
      fc::ecc::private_key child_private_key = fc::ecc::private_key::generate();
      fc::ecc::private_key nathan_private_key = fc::ecc::private_key::generate();
      const auto& child_key = register_key(child_private_key.get_public_key());
      const auto& nathan_key = register_key(nathan_private_key.get_public_key());
      const account_object& nathan = get_account("nathan");
      const account_object& root = create_account("root");

      db.modify(nathan, [nathan_key](account_object& a) {
         a.owner = authority(1, nathan_key.get_id(), 1);
         a.active = authority(1, nathan_key.get_id(), 1);
      });

      BOOST_CHECK(nathan.active.get_keys() == vector<key_id_type>{nathan_key.get_id()});

      auto op = make_account("nathan/child");
      op.registrar = root.id;
      op.owner = authority(1, child_key.get_id(), 1);
      op.active = authority(1, child_key.get_id(), 1);
      trx.operations.emplace_back(op);
      sign(trx, fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis"))));

      BOOST_REQUIRE_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, nathan_private_key);
      BOOST_REQUIRE_THROW(db.push_transaction(trx), fc::exception);
      trx.signatures.clear();
      op.owner = authority(1, account_id_type(nathan.id), 1);
      trx.operations.back() = op;
      sign(trx, fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis"))));
      sign(trx, nathan_private_key);
      db.push_transaction(trx);

      BOOST_CHECK( get_account("nathan/child").active.auths == op.active.auths );
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_account )
{
   try {
      INVOKE(create_account_test);
      const account_object& nathan = get_account("nathan");
      const fc::ecc::private_key nathan_new_key = fc::ecc::private_key::generate();
      const key_id_type key_id = db.get_index<key_object>().get_next_id();
      const auto& active_delegates = db.get_global_properties().active_delegates;

      transfer(account_id_type()(db), nathan, asset(30000));

      trx.operations.emplace_back(key_create_operation({nathan.id, asset(), address(nathan_new_key.get_public_key())}));
      db.push_transaction(trx, ~0);

      account_update_operation op;
      op.account = nathan.id;
      op.owner = authority(2, key_id, 1, key_id_type(), 1);
      op.active = authority(2, key_id, 1, key_id_type(), 1);
      op.voting_key = key_id;
      op.vote = flat_set<vote_tally_id_type>({active_delegates[0](db).vote, active_delegates[5](db).vote});
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan.voting_key == key_id);
      BOOST_CHECK(nathan.memo_key == key_id_type());
      BOOST_CHECK(nathan.active.weight_threshold == 2);
      BOOST_CHECK(nathan.active.auths.size() == 2);
      BOOST_CHECK(nathan.active.auths.at(key_id) == 1);
      BOOST_CHECK(nathan.active.auths.at(key_id_type()) == 1);
      BOOST_CHECK(nathan.owner.weight_threshold == 2);
      BOOST_CHECK(nathan.owner.auths.size() == 2);
      BOOST_CHECK(nathan.owner.auths.at(key_id) == 1);
      BOOST_CHECK(nathan.owner.auths.at(key_id_type()) == 1);
      BOOST_CHECK(nathan.votes.size() == 2);

      /** these votes are no longer tallied in real time
      BOOST_CHECK(active_delegates[0](db).vote(db).total_votes == 30000);
      BOOST_CHECK(active_delegates[1](db).vote(db).total_votes == 0);
      BOOST_CHECK(active_delegates[4](db).vote(db).total_votes == 0);
      BOOST_CHECK(active_delegates[5](db).vote(db).total_votes == 30000);
      BOOST_CHECK(active_delegates[6](db).vote(db).total_votes == 0);
      */

      transfer(account_id_type()(db), nathan, asset(3000000));

      enable_fees();
      op.upgrade_to_prime   = true;
      op.fee     = op.calculate_fee( db.get_global_properties().parameters.current_fees );
      trx.operations.push_back(op);
      db.push_transaction(trx, ~0);

      BOOST_CHECK( nathan.referrer == nathan.id );
      BOOST_CHECK( nathan.referrer_percent == 100 );
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
      trx.visit( operation_set_fee( db.current_fee_schedule() ) );

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
      trx.visit( operation_set_fee( db.current_fee_schedule() ) );

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
      trx.operations.push_back(op);

      REQUIRE_THROW_WITH_VALUE(op, delegate_account, account_id_type(99999999));
      REQUIRE_THROW_WITH_VALUE(op, fee, asset(-600));
      trx.operations.back() = op;

      delegate_id_type delegate_id = db.get_index_type<primary_index<simple_index<delegate_object>>>().get_next_id();
      db.push_transaction(trx, ~0);
      const delegate_object& d = delegate_id(db);

      BOOST_CHECK(d.delegate_account == account_id_type());
      BOOST_CHECK(d.vote(db).total_votes == 0);
   } catch (fc::exception& e) {
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
   } catch ( const fc::exception& e ) {
      elog( "${e}", ("e", e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( update_mia )
{
   try {
      INVOKE(create_mia);
      const asset_object& bit_usd = get_asset("BITUSD");

      asset_update_operation op;
      op.issuer = bit_usd.issuer;
      op.asset_to_update = bit_usd.id;
      trx.operations.emplace_back(op);

      //Cannot set core_exchange_rate on an MIA
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, price(asset(5), bit_usd.amount(5)));
      //Cannot convert an MIA to UIA
      REQUIRE_THROW_WITH_VALUE(op, flags, 0);
      REQUIRE_THROW_WITH_VALUE(op, permissions, 0);

      price_feed feed;
      feed.call_limit = price(bit_usd.amount(5), bit_usd.amount(5));
      feed.short_limit = feed.call_limit;
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed, feed);
      feed.call_limit = price(bit_usd.amount(5), asset(5));
      feed.short_limit = ~feed.call_limit;
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed, feed);
      feed.short_limit = price(asset(4), bit_usd.amount(5));
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed, feed);
      std::swap(feed.call_limit, feed.short_limit);
      op.new_price_feed = feed;
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed->max_margin_period_sec, 0);
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed->required_maintenance_collateral, 0);
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed->required_initial_collateral, 500);
      REQUIRE_THROW_WITH_VALUE(op, new_issuer, account_id_type());

      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      std::swap(op.flags, op.permissions);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      trx.operations.clear();
      auto nathan = create_account("nathan");
      op.new_issuer = nathan.id;
      trx.operations.emplace_back(op);
      db.push_transaction(trx, ~0);
      BOOST_CHECK(bit_usd.issuer == nathan.id);

      op.issuer = nathan.id;
      op.new_issuer = account_id_type();
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
   } catch ( const fc::exception& e ) {
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
      creator.permissions = ASSET_ISSUER_PERMISSION_MASK & ~market_issued;
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
      REQUIRE_THROW_WITH_VALUE(op, symbol, ".AAA");
      REQUIRE_THROW_WITH_VALUE(op, symbol, "AAA.");
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

BOOST_AUTO_TEST_CASE( update_uia )
{
   try {
      INVOKE(create_uia);
      const auto& test = get_asset("TEST");
      const auto& nathan = create_account("nathan");

      asset_update_operation op;
      op.issuer = test.issuer;
      op.permissions.reset();
      op.flags.reset();
      op.asset_to_update = test.id;

      trx.operations.push_back(op);
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      //Cannot set issuer to same value as before
      REQUIRE_THROW_WITH_VALUE(op, new_issuer, account_id_type());
      //Cannot convert to an MIA
      REQUIRE_THROW_WITH_VALUE(op, flags, market_issued);
      //Cannot set flags to same value as before
      REQUIRE_THROW_WITH_VALUE(op, flags, 0);
      //Cannot convert to an MIA
      REQUIRE_THROW_WITH_VALUE(op, permissions, ASSET_ISSUER_PERMISSION_MASK);
      //Cannot set permissions to same value as before
      REQUIRE_THROW_WITH_VALUE(op, permissions, ASSET_ISSUER_PERMISSION_MASK & ~market_issued);
      //Cannot set a price feed on a UIA
      REQUIRE_THROW_WITH_VALUE(op, new_price_feed, price_feed());
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, price(asset(5), asset(5)));

      op.core_exchange_rate = price(asset(3), test.amount(5));
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, ~*op.core_exchange_rate);
      REQUIRE_THROW_WITH_VALUE(op, core_exchange_rate, price());
      op.core_exchange_rate.reset();
      op.new_issuer = nathan.id;
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      op.issuer = nathan.id;
      op.new_issuer.reset();
      op.flags = halt_transfer | white_list;
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      REQUIRE_THROW_WITH_VALUE(op, permissions, test.issuer_permissions & ~white_list);
      op.permissions = test.issuer_permissions & ~white_list;
      op.flags = 0;
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      op.permissions.reset();
      op.flags.reset();
      BOOST_CHECK(!(test.issuer_permissions & white_list));
      REQUIRE_THROW_WITH_VALUE(op, permissions, ASSET_ISSUER_PERMISSION_MASK & ~market_issued);
      REQUIRE_THROW_WITH_VALUE(op, flags, white_list);
      op.new_issuer = account_id_type();
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      op.issuer = account_id_type();
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      op.new_issuer.reset();
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

      asset_issue_operation op({test_asset.issuer, test_asset.amount(5000000), asset(), nathan_account.id});
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
   transfer( nathan_account, buyer_account, test_asset.amount(10000),test_asset.amount(0) );

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
   transfer( nathan_account, buyer_account, test_asset.amount(10000),test_asset.amount(0) );

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

      transfer_operation op({nathan_account.id, genesis_account.id, test_asset.amount(100), test_asset.amount(0)});
      op.fee = asset(op.calculate_fee(db.current_fee_schedule())) * test_asset.core_exchange_rate;
      BOOST_CHECK(op.fee.asset_id == test_asset.id);
      asset old_balance = nathan_account.balances(db).get_balance(test_asset.id);
      asset fee = op.fee;
      BOOST_CHECK(fee.amount > 0);
      asset core_fee = fee*test_asset.core_exchange_rate;
      idump((op));
      trx.operations.push_back(std::move(op));
      db.push_transaction(trx, ~0);
      ilog(".");

      BOOST_CHECK(nathan_account.balances(db).get_balance(test_asset.id) == old_balance - fee - test_asset.amount(100));
      BOOST_CHECK(genesis_account.balances(db).get_balance(test_asset.id) == test_asset.amount(100));
      BOOST_CHECK(asset_dynamic.accumulated_fees == fee.amount);
      BOOST_CHECK(asset_dynamic.fee_pool == 1000000 - core_fee.amount);

      //Do it again, for good measure.
      ilog(".");
      db.push_transaction(trx, ~0);
      ilog(".");
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
      ilog(".");
      transfer(genesis_account, nathan_account, asset(20));
      ilog(".");
      BOOST_CHECK(nathan_account.balances(db).get_balance(asset_id_type()) == asset(20));
      idump((op));

      trx.operations.emplace_back(std::move(op));
      db.push_transaction(trx, ~0);
      ilog(".");

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

      delegate_publish_feeds_operation op({active_delegates[0], asset(), flat_set<price_feed>()});
      op.feeds.insert(price_feed());
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(30));
      op.feeds.begin()->short_limit = ~price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(10));
      // We'll expire margins after a month
      op.feeds.begin()->max_margin_period_sec = fc::days(30).to_seconds();
      // Accept defaults for required collateral
      trx.operations.emplace_back(op);
      db.push_transaction(trx, ~0);

      {
         //Dumb sanity check of some operators. Only here to improve code coverage. :D
         price_feed dummy = *op.feeds.begin();
         BOOST_CHECK(*op.feeds.begin() == dummy);
         price a(asset(1), bit_usd.amount(2));
         price b(asset(2), bit_usd.amount(2));
         price c(asset(1), bit_usd.amount(2));
         BOOST_CHECK(a < b);
         BOOST_CHECK(b > a);
         BOOST_CHECK(a == c);
         BOOST_CHECK(!(b == c));
      }

      BOOST_CHECK(bit_usd.current_feed.call_limit.to_real() == BTS_BLOCKCHAIN_PRECISION / 30.0);
      BOOST_CHECK_EQUAL(bit_usd.current_feed.short_limit.to_real(), 10.0 / BTS_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bit_usd.current_feed.max_margin_period_sec == fc::days(30).to_seconds());
      BOOST_CHECK(bit_usd.current_feed.required_initial_collateral == BTS_DEFAULT_INITIAL_COLLATERAL_RATIO);
      BOOST_CHECK(bit_usd.current_feed.required_maintenance_collateral == BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.delegate = active_delegates[1];
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(25));
      op.feeds.begin()->short_limit = ~price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(20));
      op.feeds.begin()->max_margin_period_sec = fc::days(10).to_seconds();
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK_EQUAL(bit_usd.current_feed.call_limit.to_real(), BTS_BLOCKCHAIN_PRECISION / 25.0);
      BOOST_CHECK_EQUAL(bit_usd.current_feed.short_limit.to_real(), 20.0 / BTS_BLOCKCHAIN_PRECISION);
      BOOST_CHECK(bit_usd.current_feed.max_margin_period_sec == fc::days(30).to_seconds());
      BOOST_CHECK(bit_usd.current_feed.required_initial_collateral == BTS_DEFAULT_INITIAL_COLLATERAL_RATIO);
      BOOST_CHECK(bit_usd.current_feed.required_maintenance_collateral == BTS_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

      op.delegate = active_delegates[2];
      op.feeds.begin()->call_limit = price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(40));
      op.feeds.begin()->short_limit = ~price(asset(BTS_BLOCKCHAIN_PRECISION),bit_usd.amount(10));
      op.feeds.begin()->max_margin_period_sec = fc::days(100).to_seconds();
      // But this delegate is an idiot.
      op.feeds.begin()->required_initial_collateral = 1001;
      op.feeds.begin()->required_maintenance_collateral = 1000;
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK_EQUAL(bit_usd.current_feed.call_limit.to_real(), BTS_BLOCKCHAIN_PRECISION / 30.0);
      BOOST_CHECK_EQUAL(bit_usd.current_feed.short_limit.to_real(), 10.0 / BTS_BLOCKCHAIN_PRECISION);
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

   auto& index = db.get_index_type<call_order_index>().indices().get<by_account>();
   BOOST_CHECK(index.find(boost::make_tuple(buyer_account.id, bitusd.id)) == index.end());
   BOOST_CHECK(index.find(boost::make_tuple(shorter1_account.id, bitusd.id)) != index.end());
   BOOST_CHECK(index.find(boost::make_tuple(shorter1_account.id, bitusd.id))->get_debt() == bitusd.amount(100) );
   BOOST_CHECK(index.find(boost::make_tuple(shorter1_account.id, bitusd.id))->call_price == price(asset(300), bitusd.amount(100)) );
   BOOST_CHECK(index.find(boost::make_tuple(shorter2_account.id, bitusd.id)) != index.end());
   BOOST_CHECK(index.find(boost::make_tuple(shorter2_account.id, bitusd.id))->get_debt() == bitusd.amount(100) );
   BOOST_CHECK(index.find(boost::make_tuple(shorter3_account.id, bitusd.id)) != index.end());
   BOOST_CHECK(index.find(boost::make_tuple(shorter3_account.id, bitusd.id))->get_debt() == bitusd.amount(100) );
}catch ( const fc::exception& e )
{
  elog( "${e}", ("e", e.to_detail_string() ) );
  throw;
} }

BOOST_AUTO_TEST_CASE( full_cover_test )
{
   try {
      INVOKE(multiple_shorts_matching_multiple_bids_in_order);
      const asset_object& bit_usd = get_asset("BITUSD");
      const asset_object& core = asset_id_type()(db);
      const account_object& debt_holder = get_account("shorter1");
      const account_object& usd_holder = get_account("buyer");
      auto& index = db.get_index_type<call_order_index>().indices().get<by_account>();

      BOOST_CHECK(index.find(boost::make_tuple(debt_holder.id, bit_usd.id)) != index.end());

      transfer(usd_holder, debt_holder, bit_usd.amount(100), bit_usd.amount(0));

      call_order_update_operation op;
      op.funding_account = debt_holder.id;
      op.collateral_to_add = core.amount(0);
      op.amount_to_cover = bit_usd.amount(100);

      trx.operations.push_back(op);
      REQUIRE_THROW_WITH_VALUE(op, funding_account, usd_holder.id);
      REQUIRE_THROW_WITH_VALUE(op, amount_to_cover, bit_usd.amount(-20));
      REQUIRE_THROW_WITH_VALUE(op, amount_to_cover, bit_usd.amount(200));
      REQUIRE_THROW_WITH_VALUE(op, collateral_to_add, core.amount(BTS_INITIAL_SUPPLY));
      REQUIRE_THROW_WITH_VALUE(op, collateral_to_add, bit_usd.amount(20));
      REQUIRE_THROW_WITH_VALUE(op, maintenance_collateral_ratio, 2);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK_EQUAL(get_balance(debt_holder, bit_usd), 0);
      BOOST_CHECK(index.find(boost::make_tuple(debt_holder.id, bit_usd.id)) == index.end());
   } catch( fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( partial_cover_test )
{
   try {
      INVOKE(multiple_shorts_matching_multiple_bids_in_order);
      const asset_object& bit_usd = get_asset("BITUSD");
      const asset_object& core = asset_id_type()(db);
      const account_object& debt_holder = get_account("shorter1");
      const account_object& usd_holder = get_account("buyer");
      auto& index = db.get_index_type<call_order_index>().indices().get<by_account>();
      const call_order_object& debt = *index.find(boost::make_tuple(debt_holder.id, bit_usd.id));

      BOOST_CHECK(index.find(boost::make_tuple(debt_holder.id, bit_usd.id)) != index.end());

      ilog("..." );
      transfer(usd_holder, debt_holder, bit_usd.amount(50), bit_usd.amount(0));
      ilog("..." );
      BOOST_CHECK_EQUAL(get_balance(debt_holder, bit_usd), 50);

      trx.operations.clear();
      call_order_update_operation op;
      op.funding_account = debt_holder.id;
      op.collateral_to_add = core.amount(0);
      op.amount_to_cover = bit_usd.amount(50);
      trx.operations.push_back(op);
      db.push_transaction(trx, ~0);

      BOOST_CHECK_EQUAL(get_balance(debt_holder, bit_usd), 0);
      BOOST_CHECK(index.find(boost::make_tuple(debt_holder.id, bit_usd.id)) != index.end());
      BOOST_CHECK_EQUAL(debt.debt.value, 50);
      BOOST_CHECK_EQUAL(debt.collateral.value, 400);
      BOOST_CHECK(debt.call_price == price(core.amount(300), bit_usd.amount(50)));

      op.collateral_to_add = core.amount(52);
      op.amount_to_cover = bit_usd.amount(0);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      ilog("..." );

      BOOST_CHECK(debt.call_price == price(core.amount(339), bit_usd.amount(50)));

      op.collateral_to_add = core.amount(0);
      op.amount_to_cover = bit_usd.amount(0);
      op.maintenance_collateral_ratio = 1800;
      REQUIRE_THROW_WITH_VALUE(op, maintenance_collateral_ratio, 1300);
      REQUIRE_THROW_WITH_VALUE(op, maintenance_collateral_ratio, 2500);
      op.collateral_to_add = core.amount(8);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(debt.call_price == price(core.amount(368), bit_usd.amount(50)));

      op.amount_to_cover = bit_usd.amount(50);
      op.collateral_to_add.amount = 0;
      trx.operations.back() = op;
      BOOST_CHECK_EQUAL(get_balance(debt_holder, bit_usd), 0);
      BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);

      trx.operations.clear();
      ilog("..." );
      transfer(usd_holder, debt_holder, bit_usd.amount(50), bit_usd.amount(0));
      trx.operations.clear();
      op.validate();
      trx.operations.push_back(op);
      db.push_transaction(trx, ~0);

      BOOST_CHECK(index.find(boost::make_tuple(debt_holder.id, bit_usd.id)) == index.end());
   } catch( fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

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

   transfer( genesis_account(db), shorter1, bts.amount( 10000 ) );
   transfer( genesis_account(db), shorter2, bts.amount( 10000 ) );
   transfer( genesis_account(db), shorter3, bts.amount( 10000 ) );
   transfer( genesis_account(db), buyer1, bts.amount( 10000 ) );
   transfer( genesis_account(db), buyer2, bts.amount( 10000 ) );
   transfer( genesis_account(db), buyer3, bts.amount( 10000 ) );

   // create some BitUSD
   BOOST_REQUIRE( create_sell_order( buyer1, bts.amount(1000), bitusd.amount(1000) ) );
   BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), bts.amount(1000) )   );
   BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

   // create a mixture of BitUSD sells and shorts
   BOOST_REQUIRE( create_short(      shorter1, bitusd.amount(100), bts.amount(125) )   );
   BOOST_REQUIRE( create_sell_order( buyer1,   bitusd.amount(100), bts.amount(150) )   );
   BOOST_REQUIRE( create_short(      shorter2, bitusd.amount(100), bts.amount(200) )   );
   BOOST_REQUIRE( create_sell_order( buyer1,   bitusd.amount(100), bts.amount(225) )   );
   BOOST_REQUIRE( create_short(      shorter3, bitusd.amount(100), bts.amount(250) )   );

   print_joint_market("",""); // may have bugs

   // buy up everything but the highest order
   auto unfilled_order = create_sell_order( buyer2, bts.amount(700), bitusd.amount(311) );
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

BOOST_AUTO_TEST_CASE( big_short )
{
   try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );
      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );
      const account_object& buyer3    = create_account( "buyer3" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );
      transfer( genesis_account(db), buyer3, asset( 10000 ) );

      create_sell_order(buyer1, bts.amount(500), bitusd.amount(500));
      create_sell_order(buyer2, bts.amount(500), bitusd.amount(600));
      auto unmatched_buy3 = create_sell_order(buyer3, bts.amount(500), bitusd.amount(700));

      auto unmatched = create_short(shorter1, bitusd.amount(1300), bts.amount(800));
      if( unmatched ) wdump((*unmatched));

      BOOST_CHECK( !unmatched );
      BOOST_CHECK( unmatched_buy3 );
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_for_sale().amount.value, 358);
      // The extra 1 is rounding leftovers; it has to go somewhere.
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_to_receive().amount.value, 501);
      // All three buyers offered 500 BTS for varying numbers of dollars.
      BOOST_CHECK_EQUAL(get_balance(buyer1, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer3, bts), 9500);
      // Sans the 1% market fee, buyer1 got 500 USD, buyer2 got 600 USD
      BOOST_CHECK_EQUAL(get_balance(buyer1, bitusd), 495);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bitusd), 594);
      // Buyer3 wanted 700 USD, but the shorter only had 1300-500-600=200 left, so buyer3 got 200.
      BOOST_CHECK_EQUAL(get_balance(buyer3, bitusd), 198);
      // Shorter1 never had any USD, so he shouldn't have any now. He paid 800 BTS, so he should have 9200 left.
      BOOST_CHECK_EQUAL(get_balance(shorter1, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(shorter1, bts), 9200);

      const auto& call_index = db.get_index_type<call_order_index>().indices().get<by_account>();
      const auto call_itr = call_index.find(boost::make_tuple(shorter1.id, bitusd.id));
      BOOST_CHECK(call_itr != call_index.end());
      const call_order_object& call_object = *call_itr;
      BOOST_CHECK(call_object.borrower == shorter1.id);
      //  800 from shorter1, 500 from buyer1 and buyer2 each, 500/700*200 from buyer3 totals 1942
      BOOST_CHECK_EQUAL(call_object.collateral.value, 1942);
      // Shorter1 sold 1300 USD. Make sure that's recorded accurately.
      BOOST_CHECK_EQUAL(call_object.debt.value, 1300);
      // 13 USD was paid in market fees.
      BOOST_CHECK_EQUAL(bitusd.dynamic_asset_data_id(db).accumulated_fees.value, 13);
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( big_short2 )
{
   try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );
      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );
      const account_object& buyer3    = create_account( "buyer3" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );
      transfer( genesis_account(db), buyer3, asset( 10000 ) );

      create_sell_order(buyer1, bts.amount(500), bitusd.amount(500));
      create_sell_order(buyer2, bts.amount(500), bitusd.amount(600));
      auto unmatched_buy3 = create_sell_order(buyer3, bts.amount(500), bitusd.amount(700));

      //We want to perfectly match the first two orders, so that's 1100 USD at 500/600 = 916
      auto unmatched = create_short(shorter1, bitusd.amount(1100), bts.amount(916));
      if( unmatched ) wdump((*unmatched));

      BOOST_CHECK( !unmatched );
      BOOST_CHECK( unmatched_buy3 );
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_for_sale().amount.value, 500);
      // The extra 1 is rounding leftovers; it has to go somewhere.
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_to_receive().amount.value, 700);
      // All three buyers offered 500 BTS for varying numbers of dollars.
      BOOST_CHECK_EQUAL(get_balance(buyer1, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer3, bts), 9500);
      // Sans the 1% market fee, buyer1 got 500 USD, buyer2 got 600 USD
      BOOST_CHECK_EQUAL(get_balance(buyer1, bitusd), 495);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bitusd), 594);
      // Buyer3's order wasn't matched. He should have no USD.
      BOOST_CHECK_EQUAL(get_balance(buyer3, bitusd), 0);
      // Shorter1 never had any USD, so he shouldn't have any now. He paid 916 BTS, so he should have 9084 left.
      BOOST_CHECK_EQUAL(get_balance(shorter1, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(shorter1, bts), 9084);

      const auto& call_index = db.get_index_type<call_order_index>().indices().get<by_account>();
      const auto call_itr = call_index.find(boost::make_tuple(shorter1.id, bitusd.id));
      BOOST_CHECK(call_itr != call_index.end());
      const call_order_object& call_object = *call_itr;
      BOOST_CHECK(call_object.borrower == shorter1.id);
      // 916 from shorter1, 500 from buyer1 and buyer2 each adds to 1916
      BOOST_CHECK_EQUAL(call_object.collateral.value, 1916);
      // Shorter1 sold 1100 USD. Make sure that's recorded accurately.
      BOOST_CHECK_EQUAL(call_object.debt.value, 1100);
      // 11 USD was paid in market fees.
      BOOST_CHECK_EQUAL(bitusd.dynamic_asset_data_id(db).accumulated_fees.value, 11);
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( big_short3 )
{
   try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );
      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );
      const account_object& buyer3    = create_account( "buyer3" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );
      transfer( genesis_account(db), buyer3, asset( 10000 ) );

      create_short(shorter1, bitusd.amount(1300), bts.amount(800));

      print_joint_market("","");

      create_sell_order(buyer1, bts.amount(500), bitusd.amount(500));
      create_sell_order(buyer2, bts.amount(500), bitusd.amount(600));
      auto unmatched_buy3 = create_sell_order(buyer3, bts.amount(500), bitusd.amount(700));

      print_joint_market("","");

      BOOST_CHECK( unmatched_buy3 );
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_for_sale().amount.value, 500);
      BOOST_CHECK_EQUAL( unmatched_buy3->amount_to_receive().amount.value, 700);
      BOOST_CHECK_EQUAL(get_balance(buyer1, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer3, bts), 9500);
      BOOST_CHECK_EQUAL(get_balance(buyer1, bitusd), 804);
      BOOST_CHECK_EQUAL(get_balance(buyer2, bitusd), 484);
      BOOST_CHECK_EQUAL(get_balance(buyer3, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(shorter1, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(shorter1, bts), 9200);

      const auto& call_index = db.get_index_type<call_order_index>().indices().get<by_account>();
      const auto call_itr = call_index.find(boost::make_tuple(shorter1.id, bitusd.id));
      BOOST_CHECK(call_itr != call_index.end());
      const call_order_object& call_object = *call_itr;
      BOOST_CHECK(call_object.borrower == shorter1.id);
      BOOST_CHECK_EQUAL(call_object.collateral.value, 1600);
      BOOST_CHECK_EQUAL(call_object.debt.value, 1300);
      BOOST_CHECK_EQUAL(bitusd.dynamic_asset_data_id(db).accumulated_fees.value, 12);
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
  * Originally, this test exposed a bug in vote tallying causing the total number of votes to exceed the number of
  * voting shares. This bug was resolved in commit 489b0dafe981c3b96b17f23cfc9ddc348173c529
  */
BOOST_AUTO_TEST_CASE(break_vote_count)
{
   try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& core        = get_asset( BTS_SYMBOL );
      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& buyer1    = create_account( "buyer1" );

      transfer( genesis_account(db), shorter1, asset( 100000000 ) );
      transfer( genesis_account(db), buyer1, asset( 100000000 ) );

      create_short(shorter1, bitusd.amount(1300), core.amount(800));

      create_sell_order(buyer1, core.amount(500), bitusd.amount(500));

      BOOST_CHECK_EQUAL(get_balance(buyer1, core), 99999500);
      BOOST_CHECK_EQUAL(get_balance(buyer1, bitusd), 804);
      BOOST_CHECK_EQUAL(get_balance(shorter1, bitusd), 0);
      BOOST_CHECK_EQUAL(get_balance(shorter1, core), 99999200);

      create_sell_order(shorter1, core.amount(90000000), bitusd.amount(1));
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  Create an order such that when the trade executes at the
 *  requested price the resulting payout to one party is 0
 *
 * I am unable to actually create such an order; I'm not sure it's possible. What I have done is create an order which
 * broke an assert in the matching algorithm.
 */
BOOST_AUTO_TEST_CASE( trade_amount_equals_zero )
{
   try {
      INVOKE(issue_uia);
      const asset_object& test = get_asset( "TEST" );
      const asset_object& core = get_asset( BTS_SYMBOL );
      const account_object& core_seller = create_account( "shorter1" );
      const account_object& core_buyer = get_account("nathan");

      transfer( genesis_account(db), core_seller, asset( 100000000 ) );

      BOOST_CHECK_EQUAL(get_balance(core_buyer, core), 0);
      BOOST_CHECK_EQUAL(get_balance(core_buyer, test), 10000000);
      BOOST_CHECK_EQUAL(get_balance(core_seller, test), 0);
      BOOST_CHECK_EQUAL(get_balance(core_seller, core), 100000000);

      ilog( "=================================== START===================================\n\n");
      create_sell_order(core_seller, core.amount(1), test.amount(900000));
      ilog( "=================================== STEP===================================\n\n");
      create_sell_order(core_buyer, test.amount(900001), core.amount(1));
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( margin_call_limit_test )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& core         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = core.amount(3) / bitusd.amount(1);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

      const auto& call_index = db.get_index_type<call_order_index>().indices().get<by_account>();
      const auto call_itr = call_index.find(boost::make_tuple(shorter1.id, bitusd.id));
      BOOST_REQUIRE( call_itr != call_index.end() );
      const call_order_object& call = *call_itr;
      BOOST_CHECK(call.get_collateral() == core.amount(2000));
      BOOST_CHECK(call.get_debt() == bitusd.amount(1000));
      BOOST_CHECK(call.call_price == price(core.amount(1500), bitusd.amount(1000)));
      BOOST_CHECK_EQUAL(get_balance(shorter1, core), 9000);

      ilog( "=================================== START===================================\n\n");
      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover
      auto unmatched = create_sell_order( buyer1, bitusd.amount(495), core.amount(750) );
      if( unmatched ) edump((*unmatched));
      BOOST_CHECK( !unmatched );
      BOOST_CHECK(call.get_debt() == bitusd.amount(505));
      BOOST_CHECK(call.get_collateral() == core.amount(1250));

      auto below_call_price = create_sell_order(buyer1, bitusd.amount(200), core.amount(1));
      BOOST_REQUIRE(below_call_price);
      auto above_call_price = create_sell_order(buyer1, bitusd.amount(200), core.amount(303));
      BOOST_REQUIRE(above_call_price);
      auto above_id = above_call_price->id;

      cancel_limit_order(*below_call_price);
      BOOST_CHECK_THROW(db.get_object(above_id), fc::exception);
      BOOST_CHECK(call.get_debt() == bitusd.amount(305));
      BOOST_CHECK(call.get_collateral() == core.amount(947));

      below_call_price = create_sell_order(buyer1, bitusd.amount(200), core.amount(1));
      BOOST_REQUIRE(below_call_price);
      auto below_id = below_call_price->id;
      above_call_price = create_sell_order(buyer1, bitusd.amount(95), core.amount(144));
      BOOST_REQUIRE(above_call_price);
      above_id = above_call_price->id;
      auto match_below_call = create_sell_order(buyer2, core.amount(1), bitusd.amount(200));
      BOOST_CHECK(!match_below_call);

      BOOST_CHECK_THROW(db.get_object(above_id), fc::exception);
      BOOST_CHECK_THROW(db.get_object(below_id), fc::exception);
      BOOST_CHECK(call.get_debt() == bitusd.amount(210));
      BOOST_CHECK(call.get_collateral() == core.amount(803));
   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( margin_call_limit_test_protected )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = bts.amount(1) / bitusd.amount(1);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

      ilog( "=================================== START===================================\n\n");
      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover
      auto unmatched = create_sell_order( buyer1, bitusd.amount(990), bts.amount(1500) );
      if( unmatched ) edump((*unmatched));
      BOOST_REQUIRE( unmatched );

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( dont_margin_call_limit_test )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = bts.amount(3) / bitusd.amount(1);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover
      auto unmatched = create_sell_order( buyer1, bitusd.amount(990), bts.amount(1100) );
      if( unmatched ) edump((*unmatched));
      BOOST_REQUIRE( unmatched );

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( margin_call_short_test )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = bts.amount(3) / bitusd.amount(1);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee
      ilog( "=================================== START===================================\n\n");

      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover
      auto unmatched = create_short( buyer1, bitusd.amount(990), bts.amount(1500) );
      if( unmatched ) edump((*unmatched));
      BOOST_REQUIRE( !unmatched );

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( margin_call_short_test_limit_protected )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = bts.amount(3) / bitusd.amount(4);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee
      ilog( "=================================== START===================================\n\n");

      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover
      auto unmatched = create_short( buyer1, bitusd.amount(990), bts.amount(1500) );
      if( unmatched ) edump((*unmatched));
      BOOST_REQUIRE( unmatched );

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  Create an order that cannot be filled immediately and have the
 *  transaction fail.
 */
BOOST_AUTO_TEST_CASE( limit_order_fill_or_kill )
{ try {
   INVOKE(issue_uia);
   const account_object& nathan = get_account("nathan");
   const asset_object& test = get_asset("TEST");
   const asset_object& core = asset_id_type()(db);

   limit_order_create_operation op;
   op.seller = nathan.id;
   op.amount_to_sell = test.amount(500);
   op.min_to_receive = core.amount(500);
   op.fill_or_kill = true;

   trx.operations.clear();
   trx.operations.push_back(op);
   BOOST_CHECK_THROW(db.push_transaction(trx, ~0), fc::exception);
   op.fill_or_kill = false;
   trx.operations.back() = op;
   db.push_transaction(trx, ~0);
} FC_LOG_AND_RETHROW() }

/// Shameless code coverage plugging. Otherwise, these calls never happen.
BOOST_AUTO_TEST_CASE( fill_order )
{ try {
   fill_order_operation o;
   flat_set<account_id_type> auths;
   o.get_required_auth(auths, auths);
   BOOST_CHECK_THROW(o.validate(), fc::exception);
   o.calculate_fee(db.current_fee_schedule());
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( witness_withdraw_pay_test )
{ try {
   generate_block();

   // Make an account and upgrade it to prime, so that witnesses get some pay
   create_account("nathan");
   transfer(account_id_type()(db), get_account("nathan"), asset(10000000000));
   generate_block();

   const asset_object* core = &asset_id_type()(db);
   const account_object* nathan = &get_account("nathan");
   enable_fees(100000000);
   BOOST_CHECK_GT(db.current_fee_schedule().at(prime_upgrade_fee_type).value, 0);

   account_update_operation uop;
   uop.account = nathan->get_id();
   uop.upgrade_to_prime = true;
   trx.operations.push_back(uop);
   trx.visit(operation_set_fee(db.current_fee_schedule()));
   trx.validate();
   trx.sign(generate_private_key("genesis"));
   db.push_transaction(trx);
   trx.clear();
   BOOST_CHECK_LT(get_balance(*nathan, *core), 10000000000);

   generate_block();
   nathan = &get_account("nathan");
   core = &asset_id_type()(db);
   const witness_object* witness = &db.fetch_block_by_number(db.head_block_num())->witness(db);

   BOOST_CHECK_GT(core->dynamic_asset_data_id(db).accumulated_fees.value, 0);
   BOOST_CHECK_GT(witness->accumulated_income.value, 0);

   // Withdraw the witness's pay
   enable_fees(1);
   witness_withdraw_pay_operation wop;
   wop.from_witness = witness->id;
   wop.to_account = witness->witness_account;
   wop.amount = witness->accumulated_income;
   trx.operations.push_back(wop);
   REQUIRE_THROW_WITH_VALUE(wop, amount, witness->accumulated_income.value * 2);
   trx.operations.back() = wop;
   trx.visit(operation_set_fee(db.current_fee_schedule()));
   trx.validate();
   trx.sign(generate_private_key("genesis"));
   db.push_transaction(trx);
   trx.clear();

   BOOST_CHECK_EQUAL(get_balance(witness->witness_account(db), *core), wop.amount.value - 1/*fee*/);
   BOOST_CHECK_EQUAL(witness->accumulated_income.value, 0);
} FC_LOG_AND_RETHROW() }

/**
 *  To have a secure random number we need to ensure that the same
 *  delegate does not get to produce two blocks in a row.  There is
 *  always a chance that the last delegate of one round will be the
 *  first delegate of the next round.
 *
 *  This means that when we shuffle delegates we need to make sure
 *  that there is at least N/2 delegates between consecutive turns
 *  of the same delegate.    This means that durring the random
 *  shuffle we need to restrict the placement of delegates to maintain
 *  this invariant.
 */
BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( delegate_groups_test, 1 )
BOOST_AUTO_TEST_CASE( delegate_groups_test )
{
   assert( !"not implemented" );
}

/**
 * This test demonstrates how using the call_order_update_operation to
 * increase the maintenance collateral ratio above the current market
 * price, perhaps setting it to infinity.
 */
BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( cover_with_collateral_test, 1 )
BOOST_AUTO_TEST_CASE( cover_with_collateral_test )
{
   assert( !"not implemented" );
}

/**
 *  Make sure witness pay equals a percent of accumulated fees rather than
 *  a fixed amount.  This percentage should be a PARAMTER on the order of
 *  0.00001% of accumulated fees per block.
 */
BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( witness_pay_test, 1 )
BOOST_AUTO_TEST_CASE( witness_pay_test )
{
   assert( !"not implemented" );
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( bulk_discount_test, 1 )
BOOST_AUTO_TEST_CASE( bulk_discount_test )
{
   const account_object& shorter1  = create_account( "alice" );
   const account_object& shorter2  = create_account( "bob" );
   assert( !"not implemented" );
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( margin_call_black_swan, 1 )
BOOST_AUTO_TEST_CASE( margin_call_black_swan )
{ try {
      const asset_object& bitusd      = create_bitasset( "BITUSD" );
      const asset_object& bts         = get_asset( BTS_SYMBOL );

      db.modify( bitusd, [&]( asset_object& usd ){
                 usd.current_feed.call_limit = bts.amount(30) / bitusd.amount(1);
                 });

      const account_object& shorter1  = create_account( "shorter1" );
      const account_object& shorter2  = create_account( "shorter2" );
      const account_object& buyer1    = create_account( "buyer1" );
      const account_object& buyer2    = create_account( "buyer2" );

      transfer( genesis_account(db), shorter1, asset( 10000 ) );
      transfer( genesis_account(db), shorter2, asset( 10000 ) );
      transfer( genesis_account(db), buyer1, asset( 10000 ) );
      transfer( genesis_account(db), buyer2, asset( 10000 ) );

      BOOST_REQUIRE( create_sell_order( buyer1, asset(1000), bitusd.amount(1000) ) );
      BOOST_REQUIRE( !create_short( shorter1, bitusd.amount(1000), asset(1000) )   );
      BOOST_REQUIRE_EQUAL( get_balance(buyer1, bitusd), 990 ); // 1000 - 1% fee

      ilog( "=================================== START===================================\n\n");
      // this should cause the highest bid to below the margin call threshold
      // which means it should be filled by the cover, except the cover does not
      // have enough collateral and thus a black swan event should occur.
      auto unmatched = create_sell_order( buyer1, bitusd.amount(990), bts.amount(5000) );
      if( unmatched ) edump((*unmatched));
      /** black swans should cause all of the bitusd to be converted into backing
       * asset at the price of the least collateralized call position at the time. This
       * means that this sell order would be removed.
       */
      BOOST_REQUIRE( !unmatched );

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 *  Assume the referrer gets 99% of transaction fee
 */
BOOST_AUTO_TEST_CASE( transfer_cashback_test )
{
   try {
   generate_blocks(1);

   const account_object& sam  = create_account( "sam" );
   transfer(account_id_type()(db), sam, asset(30000));
   upgrade_to_prime(sam);

   ilog( "Creating alice" );
   const account_object& alice  = create_account( "alice", sam, sam, 0 );
   ilog( "Creating bob" );
   const account_object& bob    = create_account( "bob", sam, sam, 0 );

   transfer(account_id_type()(db), alice, asset(300000));

   enable_fees();

   transfer(alice, bob, asset(100000));
   wdump((alice)(bob)(sam));
   wdump((alice.balances(db))(bob.balances(db))(sam.balances(db)));

   BOOST_REQUIRE_EQUAL( alice.balances(db).lifetime_fees_paid.value, BTS_BLOCKCHAIN_PRECISION  );

   const asset_dynamic_data_object& core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);
   // 1% of fee goes to witnesses
   BOOST_CHECK_EQUAL(core_asset_data.accumulated_fees.value, BTS_BLOCKCHAIN_PRECISION/100/*witness*/ + BTS_BLOCKCHAIN_PRECISION/5 /*burn*/);
   // 99% of fee goes to referrer / registrar sam
   BOOST_CHECK_EQUAL( sam.balances(db).cashback_rewards.value,  BTS_BLOCKCHAIN_PRECISION - BTS_BLOCKCHAIN_PRECISION/100/*witness*/  - BTS_BLOCKCHAIN_PRECISION/5/*burn*/);

   } catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
