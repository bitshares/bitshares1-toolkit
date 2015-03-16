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
      for( const account_balance_object& a : balance_index )
         for( const auto& balance : a.balances )
            total_balances[balance.first] += balance.second;
      for( const limit_order_object& o : db.get_index_type<limit_order_index>().indices() )
         total_balances[o.amount_for_sale().asset_id] += o.amount_for_sale().amount;
      for( const short_order_object& o : db.get_index_type<short_order_index>().indices() )
      {
         total_balances[o.get_collateral().asset_id] += o.get_collateral().amount;
      }
      for( const asset_object& asset_obj : db.get_index_type<asset_index>().indices() )
      {
         total_balances[asset_obj.id] += asset_obj.dynamic_asset_data_id(db).accumulated_fees;
         if( asset_obj.id != asset_id_type() )
            BOOST_CHECK(total_balances[asset_obj.id] == asset_obj.dynamic_asset_data_id(db).current_supply);
         total_balances[asset_id_type()] += asset_obj.dynamic_asset_data_id(db).fee_pool;
      }

      BOOST_CHECK( total_balances[asset_id_type()] == core_asset_data.current_supply );
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
                                           uint16_t maitenance_collateral_ratio = 1750 )
   {
      short_order_create_operation op;
      op.seller = seller.id;
      op.amount_to_sell = amount_to_sell;
      op.collateral = collateral_provided;
      op.initial_collateral_ratio = initial_collateral_ratio;
      op.maitenance_collateral_ratio = maitenance_collateral_ratio;
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
         cout << std::setw( 10 ) << std::right  << cur->short_price.to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (~cur->short_price).to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (cur->call_price).to_real() << " ";
         cout << std::setw( 10 ) << std::right  << (cur->initial_collateral_ratio)/double(1000) << " ";
         cout << std::setw( 10 ) << std::right  << (cur->maitenance_collateral_ratio)/double(1000) << " ";
         cout << "\n";
         ++cur;
      }
   }
};

} }
