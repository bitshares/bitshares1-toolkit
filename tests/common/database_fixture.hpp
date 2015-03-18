#pragma once

#include <bts/chain/database.hpp>
#include <bts/chain/simple_index.hpp>
#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/short_order_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>

using std::cout;

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
#define INVOKE(test) ((test*)this)->test_method(); RESET(trx); trx.relative_expiration = 1000

namespace bts { namespace chain {

struct database_fixture {
   database db;
   signed_transaction trx;
   key_id_type genesis_key;
   fc::ecc::private_key private_key = fc::ecc::private_key::generate();

   void verify_asset_supplies()
   {
      const asset_dynamic_data_object& core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);
      BOOST_CHECK(core_asset_data.current_supply == BTS_INITIAL_SUPPLY);
      BOOST_CHECK(core_asset_data.fee_pool == 0);

      const simple_index<account_balance_object>& balance_index = db.get_index_type<simple_index<account_balance_object>>();
      map<asset_id_type,share_type> total_balances;
      map<asset_id_type,share_type> total_debts;
      share_type core_in_orders;
      share_type reported_core_in_orders;

      for( const account_balance_object& a : balance_index )
      {
         for( const auto& balance : a.balances )
         {
            total_balances[balance.first] += balance.second;
         }
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
      for( auto item : total_debts )
      {
         //wdump( (item.first(db).dynamic_asset_data_id(db).current_supply)(item.second) );
         BOOST_CHECK_EQUAL(item.first(db).dynamic_asset_data_id(db).current_supply.value, item.second.value);
      }
      // wdump( (core_in_orders)(reported_core_in_orders) );
      BOOST_CHECK_EQUAL( core_in_orders.value , reported_core_in_orders.value );

      //wdump( (core_asset_data.current_supply)(total_balances[asset_id_type()]) );
      BOOST_CHECK_EQUAL( total_balances[asset_id_type()].value , core_asset_data.current_supply.value );
   }

   database_fixture()
   {
      db.init_genesis();
      genesis_key(db); // attempt to deref
      trx.relative_expiration = 1000;
   }
   ~database_fixture(){
      verify_asset_supplies();
   }

   account_create_operation make_account( const std::string& name = "nathan", key_id_type key = key_id_type() ) {
      account_create_operation create_account;
      create_account.fee_paying_account = account_id_type();

      create_account.name = name;
      create_account.owner.add_authority(key, 123);
      create_account.active.add_authority(key, 321);
      create_account.memo_key = key;
      create_account.voting_key = key;

      create_account.fee = create_account.calculate_fee(db.current_fee_schedule());
      return create_account;
   }
   account_id_type genesis_account;

   const asset_object& get_asset( const string& symbol )
   {
      return *db.get_index_type<asset_index>().indices().get<by_symbol>().find(symbol);
   }

   const account_object& get_account( const string& name )
   {
      return *db.get_index_type<account_index>().indices().get<by_name>().find(name);
   }

   const asset_object& create_bitasset( const string& name )
   {
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      creator.symbol = name;
      creator.max_supply = 0;
      creator.precision = 2;
      creator.market_fee_percent = BTS_MAX_MARKET_FEE_PERCENT/100; /*1%*/
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

   const account_object& create_account( const string& name )
   {
      trx.operations.push_back(make_account(name));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.get<account_object>(r.operation_results[0].get<object_id_type>());
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
      trx.operations.push_back(cancel_order);
      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      auto processed = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return processed.operation_results[0].get<asset>();
   }

   void transfer( const account_object& from, const account_object& to, const asset& amount, const asset& fee = asset() )
   {
      trx.operations.push_back(transfer_operation({from.id, to.id, amount, fee, vector<char>() }));

      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.operations.clear();
   }
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
            gpo.current_fees.at(i) = fee;
      });

   }

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
};

} }
