#include <bts/app/api.hpp>
#include <bts/app/application.hpp>
#include <bts/chain/database.hpp>
#include <bts/utilities/key_conversion.hpp>

namespace bts { namespace app {

    database_api::database_api( bts::chain::database& db ):_db(db){}

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
    signed_transaction  login_api::sign_transaction( signed_transaction trx, const vector< string >& wif_keys )const
    {
        if( trx.ref_block_num == 0 )
           trx.set_expiration( _app.chain_database()->head_block_id() );
        for( auto wif_key : wif_keys )
        {
            auto key = utilities::wif_to_key( wif_key );
            FC_ASSERT( key.valid() );
            trx.sign( *key );
        }

        return trx;
    }

} } // bts::app
