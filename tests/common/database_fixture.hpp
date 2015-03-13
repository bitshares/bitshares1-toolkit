#pragma once

#include <bts/chain/database.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/test/unit_test.hpp>

namespace bts { namespace chain {

struct database_fixture {
   database db;
   signed_transaction trx;
   key_id_type genesis_key;
   fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::digest("genesis"));
   fc::ecc::private_key private_key = fc::ecc::private_key::generate();

   database_fixture()
   {
      db.init_genesis();
      genesis_key(db); // attempt to deref
      trx.relative_expiration = 1000;
   }
   ~database_fixture(){}

   void initialize_transaction()
   {
      trx = decltype(trx)();
      trx.ref_block_num = db.head_block_num();
      trx.ref_block_prefix = db.head_block_id()._hash[1];
      trx.relative_expiration = 1;
   }
   void sign_transaction()
   {
      trx.signatures.emplace_back(genesis_private_key.sign_compact(fc::digest((transaction&)trx)));
   }
   void set_fees()
   {
      for( operation& op : trx.operations )
         op.visit(operation_set_fee(db.current_fee_schedule()));
   }

   const account_object& make_account(const std::string& name = "nathan", key_id_type key = key_id_type());
};

} }
