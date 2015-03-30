#include <bts/app/api.hpp>
#include <bts/app/application.hpp>
#include <bts/chain/database.hpp>

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
    

    fc::api<database_api> login_api::login( const string& user, const string& password )
    {
       auto db_api = std::make_shared<database_api>( std::ref(*_app.chain_database()) );
       _database_api = db_api;
       auto net_api = std::make_shared<network_api>( std::ref(_app) );
       _database_api = db_api;
       _network_api  = net_api;
       return *_database_api;
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

} } // bts::app
