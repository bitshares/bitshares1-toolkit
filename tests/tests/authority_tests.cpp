#include <bts/chain/database.hpp>
#include <bts/chain/operations.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/key_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/proposal_object.hpp>
#include <bts/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

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
      db.push_transaction(trx, database::skip_transaction_dupe_check);

      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( any_two_of_three )
{
   try {
      fc::ecc::private_key nathan_key1 = fc::ecc::private_key::regenerate(fc::digest("key1"));
      const key_object& key1 = register_key(nathan_key1.get_public_key());
      fc::ecc::private_key nathan_key2 = fc::ecc::private_key::regenerate(fc::digest("key2"));
      const key_object& key2 = register_key(nathan_key2.get_public_key());
      fc::ecc::private_key nathan_key3 = fc::ecc::private_key::regenerate(fc::digest("key3"));
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
         trx.relative_expiration = 28;
         sign(trx, nathan_key1);
         db.push_transaction(trx, database::skip_transaction_dupe_check);
         trx.operations.clear();
         trx.signatures.clear();
      } FC_CAPTURE_AND_RETHROW ((nathan.active)(key1))

      transfer_operation op = {nathan.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      trx.relative_expiration = 29;
      sign(trx, nathan_key1);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, nathan_key2);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);

      trx.signatures.clear();
      trx.relative_expiration = 30;
      sign(trx, nathan_key2);
      sign(trx, nathan_key3);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1000);

      trx.signatures.clear();
      trx.relative_expiration = 31;
      sign(trx, nathan_key1);
      sign(trx, nathan_key3);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1500);

      trx.signatures.clear();
      trx.relative_expiration = 32;
      sign(trx, fc::ecc::private_key::generate());
      sign(trx, nathan_key3);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
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
         trx.relative_expiration = 11;
      }

      const account_object& child = get_account("child");
      auto old_balance = fund(child);

      transfer_operation op = {child.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      trx.relative_expiration = 12;
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      sign(trx, parent1_key);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      trx.signatures.clear();
      trx.relative_expiration = 13;
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, parent1_key);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 500);
      trx.operations.clear();
      trx.signatures.clear();
      trx.relative_expiration = 14;

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
         db.push_transaction(trx, database::skip_transaction_dupe_check);
         BOOST_REQUIRE_EQUAL(child.active.auths.size(), 3);
         trx.operations.clear();
         trx.signatures.clear();
         trx.relative_expiration = 15;
      }

      op = {child.id, account_id_type(), core.amount(500)};
      trx.operations.push_back(op);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, parent1_key);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      trx.signatures.clear();
      trx.relative_expiration = 16;
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, parent1_key);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1000);
      trx.signatures.clear();
      sign(trx, child_key);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1500);
      trx.operations.clear();
      trx.signatures.clear();
      trx.relative_expiration = 17;

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
         trx.relative_expiration = 18;
      }

      trx.operations.push_back(op);
      sign(trx, parent1_key);
      sign(trx, parent2_key);
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, grandparent_key);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2000);
      trx.operations.clear();
      trx.signatures.clear();
      trx.relative_expiration = 19;

      {
         account_update_operation op;
         op.account = grandparent.id;
         op.active = authority(1, account_id_type(), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
         trx.signatures.clear();
         trx.relative_expiration = 20;
      }

      trx.operations.push_back(op);
      sign(trx, parent2_key);
      sign(trx, grandparent_key);
      sign(trx, fc::ecc::private_key::regenerate(fc::digest("genesis")));
      //Fails due to recursion depth.
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
      sign(trx, child_key);
      db.push_transaction(trx, database::skip_transaction_dupe_check);
      BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2500);
      trx.operations.clear();
      trx.signatures.clear();
      trx.relative_expiration = 21;

      {
         account_update_operation op;
         op.account = parent1.id;
         op.active = authority(1, account_id_type(child.id), 1);
         op.owner = *op.active;
         trx.operations.push_back(op);
         db.push_transaction(trx, ~0);
         trx.operations.clear();
         trx.signatures.clear();
         trx.relative_expiration = 22;
      }

      trx.operations.push_back(op);
      sign(trx, parent2_key);
      sign(trx, parent2_key);
      sign(trx, parent2_key);
      //Fails due to recursion depth.
      BOOST_CHECK_THROW(db.push_transaction(trx, database::skip_transaction_dupe_check), fc::exception);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( proposed_single_account )
{
   using namespace bts::chain;
   try {
      INVOKE(any_two_of_three);

      fc::ecc::private_key genesis_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
      fc::ecc::private_key nathan_key1 = fc::ecc::private_key::regenerate(fc::digest("key1"));
      fc::ecc::private_key nathan_key2 = fc::ecc::private_key::regenerate(fc::digest("key2"));
      fc::ecc::private_key nathan_key3 = fc::ecc::private_key::regenerate(fc::digest("key3"));
      const account_object& nathan = get_account("nathan");
      const asset_object& core = asset_id_type()(db);

      //Following any_two_of_three, nathan's active authority is satisfied by any two of {key1,key2,key3}
      proposal_create_operation op = {account_id_type(), asset(),
                                      {{transfer_operation{nathan.id, account_id_type(), core.amount(100)}}},
                                      db.head_block_time() + fc::days(1)};
      asset nathan_start_balance = nathan.balances(db).get_balance(core.id);
      {
         flat_set<account_id_type> active_set, owner_set;
         op.get_required_auth(active_set, owner_set);
         BOOST_CHECK_EQUAL(active_set.size(), 1);
         BOOST_CHECK_EQUAL(owner_set.size(), 0);
         BOOST_CHECK(*active_set.begin() == account_id_type());

         active_set.clear();
         op.proposed_ops.front().get_required_auth(active_set, owner_set);
         BOOST_CHECK_EQUAL(active_set.size(), 1);
         BOOST_CHECK_EQUAL(owner_set.size(), 0);
         BOOST_CHECK(*active_set.begin() == nathan.id);
      }

      trx.operations.push_back(op);
      trx.set_expiration(db.head_block_id());
      sign(trx, genesis_key);
      const proposal_object& proposal = db.get<proposal_object>(db.push_transaction(trx).operation_results.front().get<object_id_type>());

      BOOST_CHECK_EQUAL(proposal.required_active_approvals.size(), 1);
      BOOST_CHECK_EQUAL(proposal.available_active_approvals.size(), 0);
      BOOST_CHECK_EQUAL(proposal.required_owner_approvals.size(), 0);
      BOOST_CHECK_EQUAL(proposal.available_owner_approvals.size(), 0);
      BOOST_CHECK(*proposal.required_active_approvals.begin() == nathan.id);

      trx.operations = {proposal_update_operation{account_id_type(), asset(), proposal.id,{nathan.id}}};
      trx.signatures = {genesis_key.sign_compact(trx.digest())};
      //Genesis may not add nathan's approval.
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      trx.operations = {proposal_update_operation{account_id_type(), asset(), proposal.id,{account_id_type()}}};
      trx.signatures = {genesis_key.sign_compact(trx.digest())};
      //Genesis has no stake in the transaction.
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);
      trx.operations = {proposal_update_operation{nathan.id, asset(), proposal.id,flat_set<account_id_type>{},flat_set<account_id_type>{},{nathan.id}}};
      trx.signatures = {nathan_key1.sign_compact(trx.digest()), nathan_key2.sign_compact(trx.digest())};
      //This transaction doesn't need nathan's owner authority.
      BOOST_CHECK_THROW(db.push_transaction(trx), fc::exception);

      trx.operations = {proposal_update_operation{nathan.id, asset(), proposal.id,{nathan.id}}};
      trx.signatures = {nathan_key3.sign_compact(trx.digest()), nathan_key2.sign_compact(trx.digest())};
      BOOST_CHECK_EQUAL(get_balance(nathan, core), nathan_start_balance.amount.value);
      db.push_transaction(trx);
      BOOST_CHECK_EQUAL(get_balance(nathan, core), nathan_start_balance.amount.value - 100);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
