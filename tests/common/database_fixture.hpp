#pragma once

#include <bts/chain/database.hpp>

#include <bts/chain/limit_order_object.hpp>
#include <bts/chain/account_object.hpp>
#include <bts/chain/asset_object.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <iomanip>

using std::cout;

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

   const account_object& create_account( const string& name )
   {
      trx.operations.push_back(make_account(name));
      trx.validate();
      auto r = db.push_transaction(trx, ~0);
      trx.operations.clear();
      return db.get<account_object>(r.operation_results[0]);
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
      return db.find<limit_order_object>( processed.operation_results[0] );
   }
   void transfer( const account_object& from, const account_object& to, const asset& amount, const asset& fee = asset() )
   {
      trx.operations.push_back(transfer_operation({from.id, to.id, amount, fee, vector<char>() }));

      for( auto& op : trx.operations ) op.visit( operation_set_fee( db.current_fee_schedule() ) );
      trx.validate();
      db.push_transaction(trx, ~0);
      trx.operations.clear();
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
      cout << std::setw(10) << "PRICE"   << " ";
      cout << std::setw(10) << "1/PRICE" << " ";
      cout << "===========================================================================================================\n";
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
};

} }
