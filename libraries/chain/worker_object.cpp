#include <bts/chain/vesting_balance_object.hpp>
#include <bts/chain/worker_object.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace chain {

void refund_worker_type::pay_worker(share_type pay, database& db)
{
   total_burned += pay;
   db.modify(db.get(asset_id_type()).dynamic_data(db), [pay](asset_dynamic_data_object& d) {
      d.current_supply -= pay;
   });
}

void vesting_balance_worker_type::pay_worker(share_type pay, database& db)
{
   db.modify(balance(db), [&](vesting_balance_object& b) {
      b.deposit(db.head_block_time(), asset(pay));
   });
}

void vesting_balance_worker_type::initializer::init(database& db, const worker_object& obj, vesting_balance_worker_type& worker) const
{
   worker.balance = db.create<vesting_balance_object>([&](vesting_balance_object& b) {
         b.owner = obj.worker_account;
         b.balance = asset(0);

         cdd_vesting_policy policy;
         policy.vesting_seconds = fc::days(pay_vesting_period_days).to_seconds();
         policy.coin_seconds_earned = 0;
         policy.coin_seconds_earned_last_update = db.head_block_time();
         b.policy = policy;
   }).id;
}

} } // bts::chain
