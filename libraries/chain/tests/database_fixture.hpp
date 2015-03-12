#pragma once

#include <bts/chain/database.hpp>

#include <boost/test/unit_test.hpp>

namespace bts { namespace chain {

struct database_fixture {
   database db;
   signed_transaction trx;
   key_id_type genesis_key;
   fc::ecc::private_key private_key = fc::ecc::private_key::generate();

   database_fixture()
   {
      db.init_genesis();
      genesis_key(db); // attempt to deref
      trx.relative_expiration = 1000;
   }
   ~database_fixture(){}

   account_create_operation make_account() {
      account_create_operation create_account;
      create_account.fee_paying_account = account_id_type();

      create_account.name = "nathan";
      create_account.owner.add_authority(genesis_key, 123);
      create_account.active.add_authority(genesis_key, 321);
      create_account.memo_key = genesis_key;
      create_account.voting_key = genesis_key;

      create_account.fee = create_account.calculate_fee(db.current_fee_schedule());
      return create_account;
   }
};

} }
