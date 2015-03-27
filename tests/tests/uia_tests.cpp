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

      asset_whitelist_operation wop({advanced.issuer, advanced.id, asset(), nathan.id, true});

      //Fail because attempting to unlist nathan who is already unlisted.
      REQUIRE_THROW_WITH_VALUE(wop, authorize_account, false);
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);
      //Fail because attempting to whitelist nathan who is already whitelisted.
      REQUIRE_THROW_WITH_VALUE(wop, authorize_account, true);

      BOOST_CHECK(nathan.is_authorized_asset(advanced.id));
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
   try {
      INVOKE(issue_whitelist_uia);
      const asset_object& advanced = get_asset("ADVANCED");
      const account_object& nathan = get_account("nathan");
      const account_object& dan = create_account("dan");

      transfer_operation op({nathan.id, dan.id, advanced.amount(100)});
      trx.operations.push_back(op);
      //Fail because dan is not whitelisted.
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);

      asset_whitelist_operation wop({advanced.issuer, advanced.id, asset(), dan.id, true});
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);
      trx.operations.back() = op;
      db.push_transaction(trx, ~0);

      BOOST_CHECK(nathan.balances(db).get_balance(advanced.id).amount == 900);
      BOOST_CHECK(dan.balances(db).get_balance(advanced.id).amount == 100);

      wop.authorize_account = false;
      wop.whitelist_account = nathan.id;
      trx.operations.back() = wop;
      db.push_transaction(trx, ~0);

      op.amount = advanced.amount(50);
      trx.operations.back() = op;
      //Fail because nathan is not whitelisted.
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
      std::swap(op.from, op.to);
      trx.operations.back() = op;
      //Fail because nathan is not whitelisted.
      BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
