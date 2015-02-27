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
      BOOST_REQUIRE(key_id_type()(db));
      genesis_key = key_id_type()(db)->id;
      db.push_undo_state();
   }
   ~database_fixture(){}

   create_account_operation make_account() {
      create_account_operation create_account;
      create_account.name = "nathan";

      create_account.paying_account = db.get_account_index().get("init0")->id;
      create_account.owner.add_authority(genesis_key, 123);
      create_account.active.add_authority(genesis_key, 321);
      create_account.memo_key = genesis_key;
      create_account.voting_key = genesis_key;
      create_account.registration_fee = asset();

      return create_account;
   }
};

} }
