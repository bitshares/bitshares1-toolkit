#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace bts::chain;

BOOST_FIXTURE_TEST_SUITE( advanced_uia_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_advanced_uia )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = "ADVANCED";
      creator.max_supply = 100000000;
      creator.precision = 2;
      creator.market_fee_percent = BTS_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.permissions = ASSET_ISSUER_PERMISSION_MASK & ~market_issued;
      creator.flags = ASSET_ISSUER_PERMISSION_MASK & ~market_issued;
      creator.core_exchange_rate = price({asset(2),asset(1)});
      creator.short_backing_asset = asset_id_type();
      creator.whitelist_authorities = creator.blacklist_authorities = {account_id_type()};
      trx.operations.push_back(std::move(creator));
      db.push_transaction(trx, ~0);

      const asset_object& test_asset = test_asset_id(db);
      BOOST_CHECK(test_asset.symbol == "ADVANCED");
      BOOST_CHECK(asset(1, test_asset_id) * test_asset.core_exchange_rate == asset(2));
      BOOST_CHECK(test_asset.enforce_white_list());
      BOOST_CHECK(test_asset.max_supply == 100000000);
      BOOST_CHECK(test_asset.short_backing_asset == asset_id_type());
      BOOST_CHECK(test_asset.market_fee_percent == BTS_MAX_MARKET_FEE_PERCENT/100);

      const asset_dynamic_data_object& test_asset_dynamic_data = test_asset.dynamic_asset_data_id(db);
      BOOST_CHECK(test_asset_dynamic_data.current_supply == 0);
      BOOST_CHECK(test_asset_dynamic_data.accumulated_fees == 0);
      BOOST_CHECK(test_asset_dynamic_data.fee_pool == 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_whitelist_uia )
{
   try {
      INVOKE(create_advanced_uia);
      const asset_object& advanced = get_asset("ADVANCED");
      const account_object& nathan = create_account("nathan");

      asset_issue_operation op({advanced.issuer, advanced.amount(1000), asset(0), nathan.id});
      trx.operations.emplace_back(op);
      //Fail because nathan is not whitelisted.
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      account_whitelist_operation wop({account_id_type(), nathan.id, account_whitelist_operation::white_listed});

      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan.is_authorized_asset(advanced));
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan.balances(db).get_balance(advanced.id).amount == 1000);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( transfer_whitelist_uia )
{
   using namespace bts;
   try {
      INVOKE(issue_whitelist_uia);
      const asset_object& advanced = get_asset("ADVANCED");
      const account_object& nathan = get_account("nathan");
      const account_object& dan = create_account("dan");

      transfer_operation op({nathan.id, dan.id, advanced.amount(100), advanced.amount(0)});
      trx.operations.push_back(op);
      //Fail because dan is not whitelisted.
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      account_whitelist_operation wop({account_id_type(), dan.id, account_whitelist_operation::white_listed});
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK_EQUAL(get_balance(nathan, advanced), 900);
      BOOST_CHECK_EQUAL(get_balance(dan, advanced), 100);

      wop.new_listing |= account_whitelist_operation::black_listed;
      wop.account_to_list = nathan.id;
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);

      op.amount = advanced.amount(50);
      trx.operations.back() = op;
      //Fail because nathan is blacklisted
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      std::swap(op.from, op.to);
      trx.operations.back() = op;
      //Fail because nathan is blacklisted
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      {
         asset_update_operation op;
         op.asset_to_update = advanced.id;
         op.new_blacklist_authorities = flat_set<account_id_type>();
         op.new_blacklist_authorities->insert(dan.id);
         trx.operations.back() = op;
         db.push_transaction(trx, ~0);
         BOOST_CHECK(advanced.blacklist_authorities.find(dan.id) != advanced.blacklist_authorities.end());
      }

      trx.operations.back() = op;
      db.push_transaction(trx, ~0);
      BOOST_CHECK_EQUAL(get_balance(nathan, advanced), 950);
      BOOST_CHECK_EQUAL(get_balance(dan, advanced), 50);

      wop.authorizing_account = dan.id;
      wop.account_to_list = nathan.id;
      wop.new_listing = account_whitelist_operation::black_listed;
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);

      trx.operations.back() = op;
      //Fail because nathan is blacklisted
      BOOST_CHECK(!nathan.is_authorized_asset(advanced));
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      //Remove nathan from genesis' whitelist, add him to dan's. This should not authorize him to hold ADVANCED.
      wop.authorizing_account = account_id_type();
      wop.account_to_list = nathan.id;
      wop.new_listing = account_whitelist_operation::no_listing;
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);
      wop.authorizing_account = dan.id;
      wop.account_to_list = nathan.id;
      wop.new_listing = account_whitelist_operation::white_listed;
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);

      trx.operations.back() = op;
      //Fail because nathan is not whitelisted
      BOOST_CHECK(!nathan.is_authorized_asset(advanced));
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_SUITE_END()
