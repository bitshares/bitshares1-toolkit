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

BOOST_FIXTURE_TEST_SUITE( authority_tests, database_fixture )

BOOST_AUTO_TEST_CASE( simple_single_signature )
{
   try {
      fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
      const key_object& key = register_key(nathan_key.get_public_key());
      const account_object& nathan = create_account("nathan", key.id);
      const asset_object& core = asset_id_type()(db);
      auto old_balance = fund(nathan);

      transfer_operation op = {nathan.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      sign(trx, nathan_key);
      db.push_transaction(trx);

      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( any_two_of_three )
{
   try {
      fc::ecc::private_key nathan_key1 = fc::ecc::private_key::generate();
      const key_object& key1 = register_key(nathan_key1.get_public_key());
      fc::ecc::private_key nathan_key2 = fc::ecc::private_key::generate();
      const key_object& key2 = register_key(nathan_key2.get_public_key());
      fc::ecc::private_key nathan_key3 = fc::ecc::private_key::generate();
      const key_object& key3 = register_key(nathan_key3.get_public_key());
      const account_object& nathan = create_account("nathan", key1.id);
      const asset_object& core = asset_id_type()(db);
      auto old_balance = fund(nathan);

      try {
         account_update_operation op;
         op.account = nathan.id;
         op.active = authority(2, key1.get_id(), 1, key2.get_id(), 1, key3.get_id(), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         sign(trx, nathan_key1);
         db.push_transaction(trx);
         trx.operations.clear();
         trx.signatures.clear();
      } FC_CAPTURE_AND_RETHROW ((nathan.active)(key1))

      transfer_operation op = {nathan.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      sign(trx, nathan_key1);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, nathan_key2);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);

      trx.signatures.clear();
      sign(trx, nathan_key2);
      sign(trx, nathan_key3);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1000);

      trx.signatures.clear();
      sign(trx, nathan_key1);
      sign(trx, nathan_key3);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1500);

      trx.signatures.clear();
      sign(trx, fc::ecc::private_key::generate());
      sign(trx, nathan_key3);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1500);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( recursive_accounts )
{
   try {
      fc::ecc::private_key parent1_key = fc::ecc::private_key::generate();
      const key_object& key1 = register_key(parent1_key.get_public_key());
      fc::ecc::private_key parent2_key = fc::ecc::private_key::generate();
      const key_object& key2 = register_key(parent2_key.get_public_key());
      const auto& core = asset_id_type()(db);

      const account_object& parent1 = create_account("parent1", key1.id);
      const account_object& parent2 = create_account("parent2", key2.id);

      {
         auto make_child_op = make_account("child");
         make_child_op.owner = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1);
         make_child_op.active = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1);
         trx.operations.push_back(make_child_op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
      }

      const account_object& child = get_account("child");
      auto old_balance = fund(child);

      transfer_operation op = {child.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      trx.signatures.clear();
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, parent1_key);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 500);
      trx.operations.clear();
      trx.signatures.clear();

      fc::ecc::private_key child_key = fc::ecc::private_key::generate();
      const key_object& child_key_obj = register_key(child_key.get_public_key());
      {
         account_update_operation op;
         op.account = child.id;
         op.active = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1,
                               child_key_obj.get_id(), 2);
         trx.operations.push_back(op);
         sign(trx, parent1_key);
         sign(trx, parent2_key);
         db.push_transaction(trx);
         BOOST_REQUIRE_EQUAL(child.active.auths.size(), 3);;
         trx.operations.clear();
         trx.signatures.clear();
      }

      op = {child.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, parent1_key);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      trx.signatures.clear();
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, parent1_key);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1000);
      trx.signatures.clear();
      sign(trx, child_key);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1500);
      trx.operations.clear();
      trx.signatures.clear();

      auto grandparent = create_account("grandparent");
      fc::ecc::private_key grandparent_key = fc::ecc::private_key::generate();
      const key_object& grandparent_key_obj = register_key(grandparent_key.get_public_key());
      {
         account_update_operation op;
         op.account = parent1.id;
         op.active = authority(1, account_id_type(grandparent.id), 1);
         trx.operations.push_back(op);
         op.account = grandparent.id;
         op.active = authority(1, grandparent_key_obj.get_id(), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
         trx.signatures.clear();
      }

      trx.operations.push_back(op);
      sign(trx, parent1_key);
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, grandparent_key);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2000);
      trx.operations.clear();
      trx.signatures.clear();

      {
         account_update_operation op;
         op.account = grandparent.id;
         op.active = authority(1, account_id_type(), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
         trx.signatures.clear();
      }

      trx.operations.push_back(op);
      sign(trx, parent2_key);
      sign(trx, grandparent_key);
      sign(trx, fc::ecc::private_key::regenerate(fc::digest("genesis")));
      //Fails due to recursion depth.
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      sign(trx, child_key);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2500);
      trx.operations.clear();
      trx.signatures.clear();

      {
         account_update_operation op;
         op.account = parent1.id;
         op.active = authority(1, account_id_type(child.id), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
         trx.signatures.clear();
      }

      trx.operations.push_back(op);
      sign(trx, parent2_key);
      sign(trx, parent2_key);
      sign(trx, parent2_key);
      //Fails due to recursion depth.
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
