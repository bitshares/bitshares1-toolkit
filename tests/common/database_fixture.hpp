#pragma once

#include <bts/account_history/account_history_plugin.hpp>
#include <bts/app/application.hpp>
#include <bts/chain/database.hpp>
#include <bts/db/simple_index.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/delegate_object.hpp>
#include <bts/chain/time.hpp>
#include <bts/chain/witness_object.hpp>
#include <bts/chain/bond_object.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>

using std::cout;
using namespace bts::db;

// See below
#define REQUIRE_OP_VALIDATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   op.validate(); \
   op.field = temp; \
}
#define REQUIRE_OP_VALIDATION_FAILURE( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   BOOST_REQUIRE_THROW( op.validate(), fc::exception ); \
   op.field = temp; \
}
#define REQUIRE_OP_EVALUATION_SUCCESS( op, field, value ) \
{ \
   const auto temp = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = temp; \
   db.push_transaction( trx, ~0 ); \
}
///Shortcut to require an exception when processing a transaction with an operation containing an expected bad value
/// Uses require insteach of check, because these transactions are expected to fail. If they don't, subsequent tests
/// may spuriously succeed or fail due to unexpected database state.
#define REQUIRE_THROW_WITH_VALUE(op, field, value) \
{ \
   auto bak = op.field; \
   op.field = value; \
   trx.operations.back() = op; \
   op.field = bak; \
   BOOST_REQUIRE_THROW(db.push_transaction(trx, ~0), fc::exception); \
}
///This simply resets v back to its default-constructed value. Requires v to have a working assingment operator and
/// default constructor.
#define RESET(v) v = decltype(v)()
///This allows me to build consecutive test cases. It's pretty ugly, but it works well enough for unit tests.
/// i.e. This allows a test on update_account to begin with the database at the end state of create_account.
#define INVOKE(test) ((struct test*)this)->test_method(); RESET(trx); trx.relative_expiration = 1000

namespace bts { namespace chain {

struct database_fixture {
   // the reason we use an app is to exercise the indexes of built-in
   //   plugins
   bts::app::application app;
   chain::database &db;
   signed_transaction trx;
   key_id_type genesis_key;
   fc::ecc::private_key private_key = fc::ecc::private_key::generate();
   fc::ecc::private_key delegate_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")) );
   optional<fc::temp_directory> data_dir;

   static fc::ecc::private_key generate_private_key(string seed)
   {
      return fc::ecc::private_key::regenerate(fc::sha256::hash(seed));
   }

   void verify_asset_supplies()
   {
      wlog("*** Begin asset supply verification ***");
      const asset_dynamic_data_object& core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);
      BOOST_CHECK(core_asset_data.current_supply +core_asset_data.burned == BTS_INITIAL_SUPPLY);
      BOOST_CHECK(core_asset_data.fee_pool == 0);

      const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
      const auto& balance_index = db.get_index_type<account_balance_index>().indices();
      const auto& settle_index = db.get_index_type<force_settlement_index>().indices();
      map<asset_id_type,share_type> total_balances;
      map<asset_id_type,share_type> total_debts;
      share_type core_in_orders;
      share_type reported_core_in_orders;

      for( const account_balance_object& b : balance_index )
         total_balances[b.asset_type] += b.balance;
      for( const force_settlement_object& s : settle_index )
         total_balances[s.balance.asset_id] += s.balance.amount;
      for( const account_statistics_object& a : statistics_index )
      {
         total_balances[asset_id_type()] += a.cashback_rewards;
         reported_core_in_orders += a.total_core_in_orders;
      }
      for( const limit_order_object& o : db.get_index_type<limit_order_index>().indices() )
      {
         auto for_sale = o.amount_for_sale();
         if( for_sale.asset_id == asset_id_type() ) core_in_orders += for_sale.amount;
         total_balances[for_sale.asset_id] += for_sale.amount;
      }
      for( const short_order_object& o : db.get_index_type<short_order_index>().indices() )
      {
         auto col = o.get_collateral();
         if( col.asset_id == asset_id_type() ) core_in_orders += col.amount;
         total_balances[col.asset_id] += col.amount;
      }
      for( const call_order_object& o : db.get_index_type<call_order_index>().indices() )
      {
         auto col = o.get_collateral();
         if( col.asset_id == asset_id_type() ) core_in_orders += col.amount;
         total_balances[col.asset_id] += col.amount;
         total_debts[o.get_debt().asset_id] += o.get_debt().amount;
      }
      for( const asset_object& asset_obj : db.get_index_type<asset_index>().indices() )
      {
         total_balances[asset_obj.id] += asset_obj.dynamic_asset_data_id(db).accumulated_fees;
         if( asset_obj.id != asset_id_type() )
            BOOST_CHECK_EQUAL(total_balances[asset_obj.id].value, asset_obj.dynamic_asset_data_id(db).current_supply.value);
         total_balances[asset_id_type()] += asset_obj.dynamic_asset_data_id(db).fee_pool;
      }
      for( const witness_object& witness_obj : db.get_index_type<simple_index<witness_object>>() )
      {
         total_balances[asset_id_type()] += witness_obj.accumulated_income;
      }
      for( const auto& bond_offer : db.get_index_type<bond_offer_index>().indices() )
      {
         total_balances[ bond_offer.amount.asset_id ] += bond_offer.amount.amount;
      }
      for( auto item : total_debts )
         BOOST_CHECK_EQUAL(item.first(db).dynamic_asset_data_id(db).current_supply.value, item.second.value);

      BOOST_CHECK_EQUAL( core_in_orders.value , reported_core_in_orders.value );
      BOOST_CHECK_EQUAL( total_balances[asset_id_type()].value , core_asset_data.current_supply.value );
      wlog("***  End  asset supply verification ***");
   }

   void verify_account_history_plugin_index()
   {
      const std::shared_ptr<bts::account_history::account_history_plugin> pin =
         app.get_plugin<bts::account_history::account_history_plugin>( "account_history" );
      if( pin->_config.accounts.size() == 0 )
      {
         vector< pair< account_id_type, address > > tuples_from_db;
         const auto& primary_account_idx = db.get_index_type<account_index>().indices().get<by_id>();
         flat_set< address > acct_addresses;
         acct_addresses.reserve( 2 * BTS_DEFAULT_MAX_AUTHORITY_MEMBERSHIP + 2 );

         for( const account_object& acct : primary_account_idx )
         {
            account_id_type account_id = acct.id;
            acct_addresses.clear();
            for( const pair< object_id_type, weight_type >& auth : acct.owner.auths )
            {
               if( auth.first.type() == key_object_type )
                  acct_addresses.insert( key_id_type( auth.first )(db).key_address() );
            }
            for( const pair< object_id_type, weight_type >& auth : acct.active.auths )
            {
               if( auth.first.type() == key_object_type )
                  acct_addresses.insert( key_id_type( auth.first )(db).key_address() );
            }
            acct_addresses.insert( acct.memo_key(db).key_address() );
            for( const address& addr : acct_addresses )
               tuples_from_db.emplace_back( account_id, addr );
         }

         vector< pair< account_id_type, address > > tuples_from_index;
         tuples_from_index.reserve( tuples_from_db.size() );
         const auto& key_account_idx =
            db.get_index_type<bts::account_history::key_account_index>()
            .indices().get<bts::account_history::by_key>();

         for( const bts::account_history::key_account_object& key_account : key_account_idx )
         {
            address addr = key_account.key;
            for( const account_id_type& account_id : key_account.account_ids )
               tuples_from_index.emplace_back( account_id, addr );
         }

         // TODO:  use function for common functionality
         {
            // due to hashed index, account_id's may not be in sorted order...
            std::sort( tuples_from_db.begin(), tuples_from_db.end() );
            size_t size_before_uniq = tuples_from_db.size();
            auto last = std::unique( tuples_from_db.begin(), tuples_from_db.end() );
            tuples_from_db.erase( last, tuples_from_db.end() );
            // but they should be unique (multiple instances of the same
            //  address within an account should have been de-duplicated
            //  by the flat_set above)
            BOOST_CHECK( tuples_from_db.size() == size_before_uniq );
         }

         {
            // (address, account) should be de-duplicated by flat_set<>
            // in key_account_object
            std::sort( tuples_from_index.begin(), tuples_from_index.end() );
            auto last = std::unique( tuples_from_index.begin(), tuples_from_index.end() );
            size_t size_before_uniq = tuples_from_db.size();
            tuples_from_index.erase( last, tuples_from_index.end() );
            BOOST_CHECK( tuples_from_index.size() == size_before_uniq );
         }

         wdump( (tuples_from_db) );
         wdump( (tuples_from_index) );

         //BOOST_CHECK_EQUAL( tuples_from_db, tuples_from_index );
         bool is_equal = true;
         is_equal &= (tuples_from_db.size() == tuples_from_index.size());
         for( size_t i=0,n=tuples_from_db.size(); i<n; i++ )
            is_equal &= (tuples_from_db[i] == tuples_from_index[i] );
         
         bool account_history_plugin_index_ok = is_equal;
         BOOST_CHECK( account_history_plugin_index_ok );
      }
      return;
   }

   database_fixture()
      : app(), db( *app.chain_database() )
   {
      app.register_plugin<bts::account_history::account_history_plugin>();
      bts::app::application::daemon_configuration cfg;
      cfg.initial_allocation = genesis_allocation();
      app.configure_without_network( cfg );

      genesis_key(db); // attempt to deref
      trx.relative_expiration = 1000;

      chain::start_simulated_time(bts::chain::now());
   }
   ~database_fixture(){
      verify_asset_supplies();
      verify_account_history_plugin_index();
      shutdown_ntp_time();

      if( data_dir )
         db.close();
   }

   void open_database()
   {
      if( !data_dir ) {
         data_dir = fc::temp_directory();
         db.open(data_dir->path());
      }
   }

   signed_block generate_block()
   {
      open_database();

      auto aw = db.get_global_properties().active_witnesses;
      advance_simulated_time_to( db.get_next_generation_time(  aw[db.head_block_num()%aw.size()] ) );
      return db.generate_block( delegate_priv_key, aw[db.head_block_num()%aw.size()], ~0 );
   }

   /**
    * @brief Generates block_count blocks
    * @param block_count number of blocks to generate
    */
   void generate_blocks(int block_count)
   {
      for( uint32_t i = 0; i < block_count; ++i )
         generate_block();
   }
   /**
    * @brief Generates blocks until the head block time matches or exceeds timestamp
    * @param timestamp target time to generate blocks until
    */
   void generate_blocks(time_point_sec timestamp)
   {
      while( db.head_block_time() < timestamp )
         generate_block();
   }

   account_create_operation make_account( const std::string& name = "nathan", key_id_type key = key_id_type() ) {
      account_create_operation create_account;
      create_account.registrar = account_id_type();

      create_account.name = name;
      create_account.owner = authority(123, key, 123);
      create_account.active = authority(321, key, 321);
      create_account.memo_key = key;

      auto& active_delegates = db.get_global_properties().active_delegates;
      if( active_delegates.size() > 0 )
      {
         set<vote_tally_id_type> votes;
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         create_account.vote = flat_set<vote_tally_id_type>(votes.begin(), votes.end());
      }

      create_account.fee = create_account.calculate_fee(db.current_fee_schedule());
      return create_account;
   }

   account_create_operation make_account( const std::string& name,
                                          const account_object& registrar,
                                          const account_object& referrer,
                                          uint8_t referrer_percent = 100,
                                          key_id_type key = key_id_type() )
   { try {
      account_create_operation          create_account;

      create_account.registrar          = registrar.id;
      create_account.referrer           = referrer.id;
      create_account.referrer_percent   = referrer_percent;

      create_account.name = name;
      create_account.owner = authority(123, key, 123);
      create_account.active = authority(321, key, 321);
      create_account.memo_key = key;

      auto& active_delegates = db.get_global_properties().active_delegates;
      if( active_delegates.size() > 0 )
      {
         set<vote_tally_id_type> votes;
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         votes.insert(active_delegates[rand() % active_delegates.size()](db).vote);
         create_account.vote = flat_set<vote_tally_id_type>(votes.begin(), votes.end());
      }

      create_account.fee = create_account.calculate_fee(db.current_fee_schedule());
      return create_account;
   } FC_CAPTURE_AND_RETHROW((name)(referrer_percent))}





   account_id_type genesis_account;

   const asset_object& get_asset( const string& symbol )
   {
      return *db.get_index_type<asset_index>().indices().get<by_symbol>().find(symbol);
   }

   const account_object& get_account( const string& name )
   {
      return *db.get_index_type<account_index>().indices().get<by_name>().find(name);
   }

   const asset_object& create_bitasset( const string& name, uint16_t market_fee_percent = 100 /*1%*/ )
   {
      asset_create_operation creator;
      creator.issuer = account_id_type(1);
      creator.fee = asset();
      creator.symbol = name;
      creator.max_supply = 0;
      creator.precision = 2;
      creator.market_fee_percent = market_fee_percent;
      creator.permissions = market_issued;
      creator.flags = market_issued;
      creator.core_exchange_rate = price({asset(1),asset(1)});
      creator.short_backing_asset = asset_id_type();
      creator.max_supply = BTS_MAX_SHARE_SUPPLY;
      trx.operations.push_back(std::move(creator));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.get<asset_object>(r.operation_results[0].get<object_id_type>());
   }

   const asset_object& create_user_issued_asset( const string& name )
   {
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = name;
      creator.max_supply = 0;
      creator.precision = 2;
      creator.core_exchange_rate = price({asset(1),asset(1)});
      creator.max_supply = BTS_MAX_SHARE_SUPPLY;
      trx.operations.push_back(std::move(creator));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.get<asset_object>(r.operation_results[0].get<object_id_type>());
   }

   void issue_uia(const account_object& recipient, asset amount)
   {
      asset_issue_operation op({amount.asset_id(db).issuer, amount, asset(), recipient.id});
      trx.validate();
      trx.operations.push_back(op);
   }

   const short_order_object* create_short( const account_object& seller,
                                           const asset& amount_to_sell,
                                           const asset& collateral_provided,
                                           uint16_t initial_collateral_ratio = 2000,
                                           uint16_t maintenance_collateral_ratio = 1750 )
   {
      short_order_create_operation op;
      op.seller = seller.id;
      op.amount_to_sell = amount_to_sell;
      op.collateral = collateral_provided;
      op.initial_collateral_ratio = initial_collateral_ratio;
      op.maintenance_collateral_ratio = maintenance_collateral_ratio;
      trx.operations.push_back(std::move(op));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.find<short_order_object>(r.operation_results[0].get<object_id_type>());
   }

   const account_object& create_account( const string& name, const key_id_type& key = key_id_type() )
   {
      trx.operations.push_back(make_account(name, key));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      auto& result = db.get<account_object>(r.operation_results[0].get<object_id_type>());
      trx.operations.clear();
      return result;
   }
   const account_object& create_account( const string& name,
                                         const account_object& registrar,
                                         const account_object& referrer,
                                         uint8_t referrer_percent = 100,
                                         const key_id_type& key = key_id_type() )
   { try {
      trx.operations.resize(1);
      trx.operations.back() = (make_account(name, registrar, referrer, referrer_percent, key));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      const auto& result = db.get<account_object>(r.operation_results[0].get<object_id_type>());
      trx.operations.clear();
      return result;
   } FC_CAPTURE_AND_RETHROW( (name)(registrar)(referrer) ) }

   const account_object& create_account( const string& name,
                                         const private_key_type& key,
                                         const account_id_type& registrar_id = account_id_type(),
                                         const account_id_type& referrer_id = account_id_type(),
                                         uint8_t referrer_percent = 100
                                       )
   { try {
      trx.operations.clear();

      key_create_operation key_op;
      key_op.fee_paying_account = registrar_id;
      key_op.key_data = public_key_type( key.get_public_key() );
      trx.operations.push_back( key_op );

      account_create_operation account_create_op;
      relative_key_id_type key_rkid(0);

      account_create_op.registrar = registrar_id;
      account_create_op.name = name;
      account_create_op.owner = authority(1234, key_rkid, 1234);
      account_create_op.active = authority(5678, key_rkid, 5678);
      account_create_op.memo_key = key_rkid;
      account_create_op.vote = flat_set<vote_tally_id_type>();
      trx.operations.push_back( account_create_op );

      trx.validate();

      processed_transaction ptx = db.push_transaction(trx, ~0);
      wdump( (ptx) );
      const account_object& result = db.get<account_object>(ptx.operation_results[1].get<object_id_type>());
      trx.operations.clear();
      return result;
   } FC_CAPTURE_AND_RETHROW( (name)(registrar_id)(referrer_id) ) }

   const delegate_object& create_delegate( const account_object& owner )
   {
      delegate_create_operation op;
      op.delegate_account = owner.id;
      trx.operations.push_back(op);
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.get<delegate_object>(r.operation_results[0].get<object_id_type>());
   }

   const key_object& register_key( const public_key_type& key )
   {
      trx.operations.push_back(key_create_operation({account_id_type(), asset(), key}));
      key_id_type new_key = db.push_transaction(trx, ~0).operation_results[0].get<object_id_type>();
      trx.operations.clear();
      return new_key(db);
   }

   const key_object& register_address( const address& addr )
   {
      trx.operations.push_back(key_create_operation({account_id_type(), asset(), addr}));
      key_id_type new_key = db.push_transaction(trx, ~0).operation_results[0].get<object_id_type>();
      trx.operations.clear();
      return new_key(db);
   }

   uint64_t fund(const account_object& account, const asset& amount = asset(500000))
   {
      transfer(account_id_type()(db), account, amount);
      return get_balance(account, amount.asset_id(db));
   }
   void sign(signed_transaction& trx, const fc::ecc::private_key& key)
   {
      trx.signatures.push_back(key.sign_compact(trx.digest()));
   }

   const limit_order_object* create_sell_order( const account_object& user, const asset& amount, const asset& recv )
   {
      limit_order_create_operation buy_order;
      buy_order.seller = user.id;
      buy_order.amount_to_sell = amount;
      buy_order.min_to_receive = recv;
      trx.operations.push_back(buy_order);
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      auto processed = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.find<limit_order_object>( processed.operation_results[0].get<object_id_type>() );
   }
   asset cancel_limit_order( const limit_order_object& order )
   {
      limit_order_cancel_operation cancel_order;
      cancel_order.fee_paying_account = order.seller;
      cancel_order.order = order.id;
      trx.operations.push_back(cancel_order);
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      auto processed = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return processed.operation_results[0].get<asset>();
   }
   asset cancel_short_order( const short_order_object& order )
   {
      short_order_cancel_operation cancel_order;
      cancel_order.fee_paying_account = order.seller;
      cancel_order.order = order.id;
      trx.operations.push_back(cancel_order);
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      auto processed = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return processed.operation_results[0].get<asset>();
   }

   void transfer( const account_object& from, const account_object& to, const asset& amount, const asset& fee = asset() )
   { try {
      trx.operations.push_back(transfer_operation({from.id, to.id, amount, fee, vector<char>() }));

      if( fee == asset() )
      {
         for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      }
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.operations.clear();
   } FC_CAPTURE_AND_RETHROW( (from.id)(to.id)(amount)(fee) ) }

   void fund_fee_pool( const account_object& from, const asset_object& asset_to_fund, const share_type amount )
   {
      trx.operations.push_back(asset_fund_fee_pool_operation({from.id, asset_to_fund.id, amount}));

      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.operations.clear();
   }
   void enable_fees( share_type fee = BTS_BLOCKCHAIN_PRECISION )
   {
      db.modify(global_property_id_type()(db), [fee](global_property_object& gpo) {
         for( int i=0; i < FEE_TYPE_COUNT; ++i)
            gpo.parameters.current_fees.set(i, fee);
         gpo.parameters.current_fees.set( prime_upgrade_fee_type, 10*fee.value );
      });

   }
   void upgrade_to_prime( const account_object& account )
   { try {
      account_update_operation op;
      op.account = account.id;
      op.upgrade_to_prime = true;
      trx.operations.emplace_back(operation(op));
      db.push_transaction( trx, ~0 );
      FC_ASSERT( account.is_prime() );
   } FC_CAPTURE_AND_RETHROW((account)) }

   void print_market( const string& syma, const string&  symb )
   {
      const auto& limit_idx = db.get_index_type<limit_order_index>();
      const auto& price_idx = limit_idx.indices().get<by_price>();

      cout << std::fixed;
      cout.precision(5);
      cout << std::setw(10) << std::left  << "NAME"      << " ";
      cout << std::setw(16) << std::right << "FOR SALE"  << " ";
      cout << std::setw(16) << std::right << "FOR WHAT"  << " ";
      cout << std::setw(10) << std::right << "PRICE"   << " ";
      cout << std::setw(10) << std::right << "1/PRICE" << "\n";
      cout << string(70, '=') << std::endl;
      auto cur = price_idx.begin();
      while( cur != price_idx.end() )
      {
         cout << std::setw( 10 ) << std::left   << cur->seller(db).name << " ";
         cout << std::setw( 10 ) << std::right  << cur->for_sale.value << " ";
         cout << std::setw( 5 )  << std::left   << cur->amount_for_sale().asset_id(db).symbol << " ";
         cout << std::setw( 10 ) << std::right  << cur->amount_to_receive().amount.value << " ";
         cout << std::setw( 5 )  << std::left   << cur->amount_to_receive().asset_id(db).symbol << " ";
         cout << std::setw( 10 ) << std::right  << cur->sell_price.to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (~cur->sell_price).to_real() << " ";
         cout << "\n";
         ++cur;
      }
   }
   string pretty( const asset& a )
   {
      std::stringstream ss;
      ss << a.amount.value << " ";
      ss << a.asset_id(db).symbol;
      return ss.str();
   }
   void print_short_order( const short_order_object& cur )
   {
      std::cout << std::setw(10) << cur.seller(db).name << " ";
      std::cout << std::setw(10) << "SHORT" << " ";
      std::cout << std::setw(16) << pretty( cur.amount_for_sale() ) << " ";
      std::cout << std::setw(16) << pretty( cur.amount_to_receive() ) << " ";
      std::cout << std::setw(16) << (~cur.sell_price).to_real() << " ";
   }

   void print_limit_order( const limit_order_object& cur )
   {
      std::cout << std::setw(10) << cur.seller(db).name << " ";
      std::cout << std::setw(10) << "LIMIT" << " ";
      std::cout << std::setw(16) << pretty( cur.amount_for_sale() ) << " ";
      std::cout << std::setw(16) << pretty( cur.amount_to_receive() ) << " ";
      std::cout << std::setw(16) << cur.sell_price.to_real() << " ";
   }
   void print_call_orders()
   {
      cout << std::fixed;
      cout.precision(5);
      cout << std::setw(10) << std::left  << "NAME"      << " ";
      cout << std::setw(10) << std::right << "TYPE"      << " ";
      cout << std::setw(16) << std::right << "DEBT"  << " ";
      cout << std::setw(16) << std::right << "COLLAT"  << " ";
      cout << std::setw(16) << std::right << "CALL PRICE"     << " ";
      cout << std::setw(16) << std::right << "~CALL PRICE"     << "\n";
      cout << string(70, '=');

      for( const call_order_object& o : db.get_index_type<call_order_index>().indices() )
      {
         std::cout << "\n";
         cout << std::setw( 10 ) << std::left   << o.borrower(db).name << " ";
         cout << std::setw( 16 ) << std::right  << pretty( o.get_debt() ) << " ";
         cout << std::setw( 16 ) << std::right  << pretty( o.get_collateral() ) << " ";
         cout << std::setw( 16 ) << std::right  << o.call_price.to_real() << " ";
         cout << std::setw( 16 ) << std::right  << (~o.call_price).to_real() << " ";
      }
         std::cout << "\n";
   }

   void print_joint_market( const string& syma, const string&  symb )
   {
      cout << std::fixed;
      cout.precision(5);

      cout << std::setw(10) << std::left  << "NAME"      << " ";
      cout << std::setw(10) << std::right << "TYPE"      << " ";
      cout << std::setw(16) << std::right << "FOR SALE"  << " ";
      cout << std::setw(16) << std::right << "FOR WHAT"  << " ";
      cout << std::setw(16) << std::right << "PRICE"     << "\n";
      cout << string(70, '=');

      const auto& limit_idx = db.get_index_type<limit_order_index>();
      const auto& limit_price_idx = limit_idx.indices().get<by_price>();
      const auto& short_idx = db.get_index_type<short_order_index>();
      const auto& sell_price_idx = short_idx.indices().get<by_price>();

      auto limit_itr = limit_price_idx.begin();
      auto short_itr = sell_price_idx.rbegin();
      while( true )
      {
         std::cout << std::endl;
         if( limit_itr != limit_price_idx.end() )
         {
            if( short_itr != sell_price_idx.rend() && limit_itr->sell_price > ~short_itr->sell_price )
            {
               print_short_order( *short_itr );
               ++short_itr;
            }
            else // print the limit
            {
               print_limit_order( *limit_itr );
               ++limit_itr;
            }
         }
         else if( short_itr != sell_price_idx.rend() )
         {
            print_short_order( *short_itr );
            ++short_itr;
         }
         else break;
      }
   }

   void print_short_market( const string& syma, const string&  symb )
   {
      const auto& limit_idx = db.get_index_type<short_order_index>();
      const auto& price_idx = limit_idx.indices().get<by_price>();

      cout << std::fixed;
      cout.precision(5);
      cout << std::setw(10) << std::left  << "NAME"        << " ";
      cout << std::setw(16) << std::right << "FOR SHORT"   << " ";
      cout << std::setw(16) << std::right << "COLLATERAL"  << " ";
      cout << std::setw(10) << std::right << "PRICE"       << " ";
      cout << std::setw(10) << std::right << "1/PRICE"     << " ";
      cout << std::setw(10) << std::right << "CALL PRICE"  << " ";
      cout << std::setw(10) << std::right << "I-Ratio"     << " ";
      cout << std::setw(10) << std::right << "M-Ratio"     << "\n";
      cout << string(100, '=') << std::endl;
      auto cur = price_idx.begin();
      while( cur != price_idx.end() )
      {
         cout << std::setw( 10 ) << std::left   << cur->seller(db).name << " ";
         cout << std::setw( 16 ) << std::right  << pretty( cur->amount_for_sale() ) << " ";
         cout << std::setw( 16 ) << std::right  << pretty( cur->get_collateral() ) << " ";
         cout << std::setw( 10 ) << std::right  << cur->sell_price.to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (~cur->sell_price).to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (cur->call_price).to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (cur->initial_collateral_ratio)/double(1000) << " ";
         cout << std::setw( 10 ) << std::right  << (cur->maintenance_collateral_ratio)/double(1000) << " ";
         cout << "\n";
         ++cur;
      }
   }

   int64_t get_balance( account_id_type account, asset_id_type a )const
   {
      return db.get_balance(account, a).amount.value;
   }
   int64_t get_balance( const account_object& account, const asset_object& a )const
   {
      return db.get_balance(account.get_id(), a.get_id()).amount.value;
   }
};

} }
