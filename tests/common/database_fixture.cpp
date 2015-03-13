#include "database_fixture.hpp"

#include <bts/chain/key_object.hpp>
#include <bts/chain/account_object.hpp>

namespace bts { namespace chain {

const account_object& database_fixture::make_account(const string& name, key_id_type key) {
   account_create_operation create_account;
   create_account.fee_paying_account = account_id_type();

   create_account.name = name;
   create_account.owner.add_authority(key, 123);
   create_account.active.add_authority(key, 321);
   create_account.memo_key = key;
   create_account.voting_key = key;

   create_account.fee = create_account.calculate_fee(db.current_fee_schedule());

   initialize_transaction();
   trx.operations.emplace_back(std::move(create_account));
   set_fees();
   trx.validate();
   auto ptrx = db.push_transaction(trx, ~0);
   initialize_transaction();

   return account_id_type(ptrx.operation_results.front())(db);
}

} } //bts::chain
