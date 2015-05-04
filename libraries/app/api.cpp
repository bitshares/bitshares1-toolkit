#include <bts/app/api.hpp>
#include <bts/app/application.hpp>
#include <bts/chain/asset_object.hpp>
#include <bts/chain/database.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/chain/operation_history_object.hpp>

namespace bts { namespace app {

    database_api::database_api( bts::chain::database& db ):_db(db)
    {
       _change_connection = _db.changed_objects.connect( [this]( const vector<object_id_type>& ids ) {
                                    on_objects_changed( ids );
                                    });
    }

    fc::variants database_api::get_objects( const vector<object_id_type>& ids )const
    {
       fc::variants result;
       result.reserve(ids.size());
       for( auto id : ids )
       {
          if( auto obj = _db.find_object(id) )
             result.push_back( obj->to_variant() );
          else
             result.push_back( fc::variant() );
       }
       return result;
    }

    optional<signed_block> database_api::get_block( uint32_t block_num )const
    {
       return _db.fetch_block_by_number( block_num );
    }

    vector<optional<account_object>>  database_api::lookup_account_names( const vector<string>& account_names )const
    {
       const auto& account_idx = _db.get_index_type<account_index>();
       const auto& accounts_by_name = account_idx.indices().get<by_name>();
       vector<optional<account_object> > result;
       result.reserve( account_names.size() );
       for( const auto& account_name : account_names )
       {
          auto itr = accounts_by_name.find( account_name );
          result.push_back( itr != accounts_by_name.end() ? *itr : optional<account_object>() );
       }
       return result;
    }

    vector<optional<asset_object>>    database_api::lookup_asset_symbols( const vector<string>& symbols )const
    {
       const auto& asset_idx = _db.get_index_type<asset_index>();
       const auto& assets_by_symbol = asset_idx.indices().get<by_symbol>();
       vector<optional<asset_object> > result;
       result.reserve( symbols.size() );
       for( const auto& symbol : symbols )
       {
          auto itr = assets_by_symbol.find( symbol );
          result.push_back( itr != assets_by_symbol.end() ? *itr : optional<asset_object>() );
       }
       return result;
    }
    global_property_object    database_api::get_global_properties()const
    {
       return _db.get( global_property_id_type() );
    }
    
    dynamic_global_property_object database_api::get_dynamic_global_properties()const
    {
       return _db.get( dynamic_global_property_id_type() );
    }

    vector<optional<key_object>>      database_api::get_keys( const vector<key_id_type>& key_ids )const
    {
       vector<optional<key_object>> result; result.reserve(key_ids.size());
       for( auto id : key_ids )
       {
          const key_object* a = _db.find(id);
          result.push_back( a ? *a : optional<key_object>() );
       }
       return result;
    }

    vector<optional<account_object>>  database_api::get_accounts( const vector<account_id_type>& account_ids )const
    {
       vector<optional<account_object>> result; result.reserve(account_ids.size());
       for( auto id : account_ids )
       {
          const account_object* a = _db.find(id);
          result.push_back( a ? *a : optional<account_object>() );
       }
       return result;
    }

    vector<optional<asset_object>>    database_api::get_assets( const vector<asset_id_type>& asset_ids )const
    {
       vector<optional<asset_object>> result; result.reserve(asset_ids.size());
       for( auto id : asset_ids )
       {
          const asset_object* a = _db.find(id);
          result.push_back( a ? *a : optional<asset_object>() );
       }
       return result;
    }

    uint64_t                      database_api::get_account_count()const 
    {
       const auto& account_idx = _db.get_index_type<account_index>();
       return account_idx.indices().size();
    }


    map<string,account_id_type>   database_api::lookup_accounts( const string& lower_bound_name, uint32_t limit )const
    {
       const auto& account_idx = _db.get_index_type<account_index>();
       const auto& accounts_by_name = account_idx.indices().get<by_name>();
       map<string,account_id_type> result;

       auto itr = accounts_by_name.lower_bound( lower_bound_name );
       while( limit && itr != accounts_by_name.end() )
       {
          result[itr->name] = itr->id; 
          ++itr;
          --limit;
       }
       return result;
    }

    vector<operation_history_object>  database_api::get_account_history( account_id_type a, 
                                                                         operation_history_id_type stop )const
    {
       vector<operation_history_object> result;
       const auto& stats = a(_db).statistics(_db);
       if( stats.most_recent_op == account_transaction_history_id_type() ) return result;
       const account_transaction_history_object* node = &stats.most_recent_op(_db);
       while( node && node->operation_id != stop )
       {
          result.push_back( node->operation_id(_db) );
          if( node->next == account_transaction_history_id_type() )
             node = nullptr;
          else node = _db.find(node->next);
       }
       return result;
    }

    vector<asset>  database_api::get_account_balances( account_id_type acnt, const flat_set<asset_id_type>& assets )const
    {
       vector<asset> result;  result.reserve( assets.size() );

       const auto& account_bal_idx   = _db.get_index_type<account_balance_index>();
       const auto& bal_by_account    = account_bal_idx.indices().get<by_account>();
       auto itr = bal_by_account.find( acnt );
       while( itr->owner == acnt )
       {
          if( assets.size() == 0 || assets.find( itr->asset_type ) != assets.end() )
                result.push_back( itr->get_balance() );
          ++itr;
       }

       return result;
    }

    /**
     *  @return the limit orders for both sides of the book for the two assets specified up to limit number on each side.
     */
    vector<limit_order_object>        database_api::get_limit_orders( asset_id_type a, asset_id_type b, uint32_t limit )const
    {
       const auto& limit_order_idx = _db.get_index_type<limit_order_index>();
       const auto& limit_price_idx = limit_order_idx.indices().get<by_price>();
       
       vector<limit_order_object>  result;
       
       int count = 0;
       auto limit_itr = limit_price_idx.lower_bound( price::max(a,b) );
       auto limit_end = limit_price_idx.upper_bound( price::min(a,b) );
       while( limit_itr != limit_end && count < limit )
       {
          result.push_back( *limit_itr );
          ++limit_itr;
          ++count;
       }
       count = 0;
       limit_itr = limit_price_idx.lower_bound( price::max(b,a) );
       limit_end = limit_price_idx.upper_bound( price::min(b,a) );
       while( limit_itr != limit_end && count < limit )
       {
          result.push_back( *limit_itr );
          ++limit_itr;
          ++count;
       }

       return result;
    }

    vector<short_order_object>        database_api::get_short_orders( asset_id_type a, uint32_t limit )const
    {
      const auto& short_order_idx = _db.get_index_type<short_order_index>();
      const auto& sell_price_idx = short_order_idx.indices().get<by_price>();
      price max_price; // TODO: define this properly.

      FC_ASSERT( max_price.max() >= max_price );
      auto short_itr = sell_price_idx.lower_bound( max_price.max() );
      auto short_end = sell_price_idx.upper_bound( max_price );

      return vector<short_order_object>();
    }

    vector<call_order_object>         database_api::get_call_orders( asset_id_type a, uint32_t limit )const
    {
       return vector<call_order_object>();
    }

    vector<force_settlement_object>   database_api::get_settle_orders( asset_id_type a, uint32_t limit )const
    {
       return vector<force_settlement_object>();
    }

    vector<asset_object> database_api::list_assets( const string& lower_bound_symbol, uint32_t limit )const
    {
       /*
        * This commented-out implementation tries to list assets
        * in alphabetical order, however it does not work
        * because asset_index is hashed, not ordered.
        *
        * So for now it just returns assets based on consecutive
        * asset_id's starting at lower_bound_symbol.
        *
        * TODO:  Fix this when wallet gets its own plugin.
        *

       const auto& asset_idx = _db.get_index_type<asset_index>();
       const auto& assets_by_symbol = asset_idx.indices().get<by_symbol>();
       map<string,asset_id_type> result;

       auto itr = assets_by_symbol.lower_bound( lower_bound_symbol );
       while( limit && itr != assets_by_symbol.end() )
       {
          result[itr->name] = itr->id;
          ++itr;
          --limit;
       }
       */
       const auto& asset_idx = _db.get_index_type<asset_index>();
       const auto& assets_by_id = asset_idx.indices().get<by_id>();
       const auto& assets_by_symbol = asset_idx.indices().get<by_symbol>();
       vector<asset_object> result;
       result.reserve( limit );

       // grab the assets starting at lower_bound_symbol
       auto itr = assets_by_symbol.find( lower_bound_symbol );
       if( itr == assets_by_symbol.end() )
          return result;

       auto id_itr = assets_by_id.find( itr->id );
       while( (limit > 0) && (id_itr != assets_by_id.end()) )
       {
          result.push_back( *id_itr );
          ++id_itr;
          --limit;
       }
       return result;
    }
    login_api::login_api( application& a )
    :_app(a)
    {
    }
    login_api::~login_api()
    {
    }

    bool login_api::login( const string& user, const string& password )
    {
       auto db_api = std::make_shared<database_api>( std::ref(*_app.chain_database()) );
       _database_api = db_api;
       auto net_api = std::make_shared<network_api>( std::ref(_app) );
       _database_api = db_api;
       _network_api  = net_api;
       return true;
    }

    void network_api::add_node( const fc::ip::endpoint& ep )
    {
       _app.p2p_node()->add_node( ep );
    }

    void network_api::broadcast_transaction( const signed_transaction& trx )
    {
       _app.chain_database()->push_transaction(trx);
       _app.p2p_node()->broadcast_transaction(trx);
    }
   
    std::vector<net::peer_status>  network_api::get_connected_peers() const
    {
      return _app.p2p_node()->get_connected_peers();
    }

    fc::api<network_api>  login_api::network()const
    {
       FC_ASSERT( _network_api );
       return *_network_api;
    }

    fc::api<database_api>  login_api::database()const
    {
       FC_ASSERT( _database_api );
       return *_database_api;
    }

    signed_transaction  login_api::sign_transaction( signed_transaction trx, const map< key_id_type,string >& wif_keys )const
    {
        if( trx.ref_block_num == 0 )
           trx.set_expiration( _app.chain_database()->head_block_id() );
        for( auto wif_key : wif_keys )
        {
            auto key = utilities::wif_to_key( wif_key.second );
            FC_ASSERT( key.valid() );
            trx.sign( wif_key.first, *key );
        }

        return trx;
    }

    string login_api::serialize_transaction( signed_transaction trx, bool hex )const
    {
        std::ostringstream ss;
        fc::raw::pack( ss, trx );
        if( hex )
        {
           std::ostringstream ss_hex;
           for( char c : ss.str() )
           {
              ss_hex << "0123456789abcdef"[ (c >> 4) & 0x0f ]
                     << "0123456789abcdef"[ (c     ) & 0x0f ];
           }
           return ss_hex.str();
        }
        return ss.str();
    }
    void database_api::on_objects_changed( const vector<object_id_type>& ids )
    {
       vector<object_id_type> my_objects;
       for( auto id : ids ) 
          if( _subscriptions.find(id) != _subscriptions.end() )
             my_objects.push_back(id);

       _broadcast_changes_complete = fc::async( [=](){
          for( auto id : my_objects )
          {
             const object* obj = _db.find_object(id);
             if( obj )
             {
                _subscriptions[id]( obj->to_variant() );
             }
          }
       });
    }
    database_api::~database_api()
    {
       wlog("${p}", ("p",int64_t(this)));
       try {
          if( _broadcast_changes_complete.valid() )
          {
             _broadcast_changes_complete.cancel();
             _broadcast_changes_complete.wait();
          }
       } catch ( const fc::exception& e )
       {
          wlog( "${e}", ("e",e.to_detail_string() ) );
       }
    }

    bool database_api::subscribe_to_objects(  const std::function<void(const fc::variant&)>&  callback, const vector<object_id_type>& ids)
    {
       for( auto id : ids ) _subscriptions[id] = callback;

       return true;
    }

    bool database_api::unsubscribe_from_objects( const vector<object_id_type>& ids )
    {
       for( auto id : ids ) _subscriptions.erase(id);

       return true;
    }

} } // bts::app
