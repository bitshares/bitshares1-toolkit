
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>

#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>

#include <bts/app/api.hpp>
#include <bts/chain/address.hpp>
#include <bts/utilities/key_conversion.hpp>

using namespace bts::app;
using namespace bts::chain;
using namespace bts::utilities;
using namespace std;

struct wallet_data
{
   flat_set<account_id_type> accounts;
   // map of key_id -> base58 private key
   map<key_id_type, string>  keys;
   // map of account_name -> base58_private_key for
   //    incomplete account regs
   map<string, string> pending_account_registrations;

   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};
FC_REFLECT( wallet_data, (accounts)(keys)(pending_account_registrations)(ws_server)(ws_user)(ws_password) );

// BLOCK  TRX  OP  VOP  
struct operation_printer
{
   operation_result _result;
   operation_printer( const operation_result& r = operation_result() ):_result(r){}
   typedef void result_type;
   template<typename T>
   void operator()( const T& op )const
   {
      balance_accumulator acc;
      op.get_balance_delta( acc, _result );
      std::cerr << fc::get_typename<T>::name() <<" ";
      std::cerr << "balance delta: " << fc::json::to_string(acc.balance) <<"   ";
      std::cerr << fc::json::to_string(op.fee_payer()) << "  fee: " << fc::json::to_string(op.fee);
   }
   void operator()( const account_create_operation& op )const
   {
      balance_accumulator acc;
      op.get_balance_delta( acc, _result );
      std::cerr << "Create Account '" << op.name << "' ";
      std::cerr << "balance delta: " << fc::json::to_string(acc.balance) <<"   ";
      std::cerr << fc::json::to_string(op.fee_payer()) << "  fee: " << fc::json::to_string(op.fee);
   }
};

/**
 *  This wallet assumes nothing about where the database server is
 *  located and performs minimal caching.  This API could be provided
 *  locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( fc::api<login_api> rapi )
      :_remote_api(rapi)
      {
         _remote_db  = _remote_api->database();
         _remote_net = _remote_api->network();
      }

      virtual ~wallet_api()
      {
         try
         {
            if( _resync_loop_task.valid() )
               _resync_loop_task.cancel_and_wait();
         }
         catch(fc::canceled_exception&)
         {
            //Expected exception. Move along.
         }
         catch(fc::exception& e)
         {
            edump((e.to_detail_string()));
         }
         return;
      }

      string  help()const;

      uint64_t  get_account_count()const
      {
         return _remote_db->get_account_count();
      }

      map<string,account_id_type> list_accounts( const string& lowerbound, uint32_t limit)
      {
         return _remote_db->lookup_accounts( lowerbound, limit );
      }
      vector<asset> list_account_balances( const account_id_type& id )
      {
         return _remote_db->get_account_balances( id, flat_set<asset_id_type>() );
      }
      vector<operation_history_object>  get_account_history( account_id_type id )const
      {
         return _remote_db->get_account_history( id, operation_history_id_type() );
      }

      string  suggest_brain_key()const
      {
        return string("dummy");
      }
      variant get_object( object_id_type id )
      {
         return _remote_db->get_objects({id});
      }
      account_object get_account( string account_name_or_id )
      {
         FC_ASSERT( account_name_or_id.size() > 0 );
         vector<optional<account_object>> opt_account;
         if( std::isdigit( account_name_or_id.front() ) )
            opt_account = _remote_db->get_accounts( {fc::variant(account_name_or_id).as<account_id_type>()} );
         else
            opt_account = _remote_db->lookup_account_names( {account_name_or_id} );
         FC_ASSERT( opt_account.size() && opt_account.front() );
         return *opt_account.front();
      }

      bool import_key( string account_name_or_id, string wif_key )
      {
         auto opt_priv_key = wif_to_key(wif_key);
         FC_ASSERT( opt_priv_key.valid() );
         bts::chain::address wif_key_address = bts::chain::address(
             opt_priv_key->get_public_key() );

         auto acnt = get_account( account_name_or_id );

         flat_set<key_id_type> keys;
         for( auto item : acnt.active.auths )
         {
             if( item.first.type() == key_object_type )
                keys.insert( item.first );
         }
         for( auto item : acnt.owner.auths )
         {
             if( item.first.type() == key_object_type )
                keys.insert( item.first );
         }
         auto opt_keys = _remote_db->get_keys( vector<key_id_type>(keys.begin(),keys.end()) );
         for( const fc::optional<key_object>& opt_key : opt_keys )
         {
            // the requested key ID's should all exist because they are
            //    keys for an account
            FC_ASSERT( opt_key.valid() );
            // we do this check by address because key objects on the
            //    blockchain may not contain a key (i.e. are simply an address)
            if( opt_key->key_address() == wif_key_address )
            {
               _wallet.keys[ opt_key->id ] = wif_key;
               return true;
            }
         }
         ilog( "key not for account ${name}", ("name",account_name_or_id) );
         return false;
      }

      string normalize_brain_key( string s )
      {
          size_t i = 0, n = s.length();
          std::string result;
          char c;
          result.reserve( n );

          bool preceded_by_whitespace = false;
          bool non_empty = false;
          while( i < n )
          {
              c = s[i++];
              switch( c )
              {
                  case ' ':  case '\t': case '\r': case '\n': case '\v': case '\f':
                      preceded_by_whitespace = true;
                      continue;

                  case 'a': c = 'A'; break;
                  case 'b': c = 'B'; break;
                  case 'c': c = 'C'; break;
                  case 'd': c = 'D'; break;
                  case 'e': c = 'E'; break;
                  case 'f': c = 'F'; break;
                  case 'g': c = 'G'; break;
                  case 'h': c = 'H'; break;
                  case 'i': c = 'I'; break;
                  case 'j': c = 'J'; break;
                  case 'k': c = 'K'; break;
                  case 'l': c = 'L'; break;
                  case 'm': c = 'M'; break;
                  case 'n': c = 'N'; break;
                  case 'o': c = 'O'; break;
                  case 'p': c = 'P'; break;
                  case 'q': c = 'Q'; break;
                  case 'r': c = 'R'; break;
                  case 's': c = 'S'; break;
                  case 't': c = 'T'; break;
                  case 'u': c = 'U'; break;
                  case 'v': c = 'V'; break;
                  case 'w': c = 'W'; break;
                  case 'x': c = 'X'; break;
                  case 'y': c = 'Y'; break;
                  case 'z': c = 'Z'; break;

                  default:
                      break;
              }
              if( preceded_by_whitespace && non_empty )
                  result.push_back(' ');
              result.push_back(c);
              preceded_by_whitespace = false;
              non_empty = true;
          }

          return result;
      }

      fc::ecc::private_key derive_private_key(
          const std::string& prefix_string, int sequence_number)
      {
           std::string sequence_string = std::to_string(sequence_number);
           fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
           fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
           return derived_key;
      }

      signed_transaction create_account_with_brain_key(
          string brain_key,
          string account_name,
          string registrar_account,
          string referrer_account,
          uint8_t referrer_percent,
          bool broadcast = false
          )
      {
        string normalized_brain_key = normalize_brain_key( brain_key );
        // TODO:  scan blockchain for accounts that exist with same brain key
        fc::ecc::private_key owner_privkey = derive_private_key( normalized_brain_key, 0 );
        fc::ecc::private_key active_privkey = derive_private_key( key_to_wif(owner_privkey), 0);

        bts::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
        bts::chain::public_key_type active_pubkey = active_privkey.get_public_key();

        account_create_operation account_create_op;

        // TODO:  process when pay_from_account is ID

        account_object registrar_account_object =
            this->get_account( registrar_account );

        account_id_type registrar_account_id = registrar_account_object.id;

        if( referrer_percent > 0 )
        {
            account_object referrer_account_object =
                this->get_account( referrer_account );
            account_create_op.referrer = referrer_account_object.id;
            account_create_op.referrer_percent = referrer_percent;
        }

        // get pay_from_account_id
        key_create_operation owner_key_create_op;
        owner_key_create_op.fee_paying_account = registrar_account_id;
        owner_key_create_op.key_data = owner_pubkey;

        key_create_operation active_key_create_op;
        active_key_create_op.fee_paying_account = registrar_account_id;
        active_key_create_op.key_data = active_pubkey;

        // key_create_op.calculate_fee(db.current_fee_schedule());

        // TODO:  Check if keys already exist!!!

        relative_key_id_type owner_rkid(0);
        relative_key_id_type active_rkid(1);

        account_create_op.registrar = registrar_account_id;
        account_create_op.name = account_name;
        account_create_op.owner = authority(1, owner_rkid, 1);
        account_create_op.active = authority(1, active_rkid, 1);
        account_create_op.memo_key = active_rkid;
        // TODO: Doesn't compile
        //account_create_op.voting_key = active_rkid;
        account_create_op.vote = flat_set<vote_tally_id_type>();

        // current_fee_schedule()
        // find_account(pay_from_account)

        // account_create_op.fee = account_create_op.calculate_fee(db.current_fee_schedule());

        signed_transaction tx;

        tx.operations.push_back( owner_key_create_op );
        tx.operations.push_back( active_key_create_op );
        tx.operations.push_back( account_create_op );

        tx.visit( operation_set_fee( _remote_db->get_global_properties().parameters.current_fees ) );

        vector<key_id_type> paying_keys = registrar_account_object.active.get_keys();

        tx.validate();

        for( key_id_type& key : paying_keys )
        {
            auto it = _wallet.keys.find(key);
            if( it != _wallet.keys.end() )
            {
                fc::optional< fc::ecc::private_key > privkey = wif_to_key( it->second );
                if( !privkey.valid() )
                {
                    FC_ASSERT( false, "Malformed private key in _wallet.keys" );
                }
                tx.sign( *privkey );
            }
        }

        // we do not insert owner_privkey here because
        //    it is intended to only be used for key recovery
        _wallet.pending_account_registrations[ account_name ] = key_to_wif( active_privkey );
        if( broadcast )
            _remote_net->broadcast_transaction( tx );
        return tx;
      }

      signed_transaction transfer( string from,
                                   string to,
                                   uint64_t amount,
                                   string asset_symbol,
                                   string memo,
                                   bool broadcast = false )
      {
        auto opt_asset = _remote_db->lookup_asset_symbols( {asset_symbol} );
        wdump( (opt_asset) );
        return signed_transaction();
      }

      // methods that start with underscore are not incuded in API
      void _resync()
      {
         // this method is used to update wallet_data annotations
         //   e.g. wallet has been restarted and was not notified
         //   of events while it was down
         //
         // everything that is done "incremental style" when a push
         //   notification is received, should also be done here
         //   "batch style" by querying the blockchain

         if( _wallet.pending_account_registrations.size() > 0 )
         {
            std::vector<string> v_names;
            v_names.reserve( _wallet.pending_account_registrations.size() );

            for( auto it : _wallet.pending_account_registrations )
               v_names.push_back( it.first );

            std::vector< fc::optional< bts::chain::account_object >>
                v_accounts = _remote_db->lookup_account_names( v_names );

            for( fc::optional< bts::chain::account_object > opt_account : v_accounts )
            {
               if( ! opt_account.valid() )
                  continue;

               string account_name = opt_account->name;
               auto it = _wallet.pending_account_registrations.find( account_name );
               FC_ASSERT( it != _wallet.pending_account_registrations.end() );
               if( import_key( account_name, it->second ) )
               {
                  ilog( "successfully imported account ${name}",
                        ("name", account_name) );
               }
               else
               {
                  // somebody else beat our pending registration, there is
                  //    nothing we can do except log it and move on
                  elog( "account ${name} registered by someone else first!",
                        ("name", account_name) );
                  // might as well remove it from pending regs,
                  //    because there is now no way this registration
                  //    can become valid (even in the extremely rare
                  //    possibility of migrating to a fork where the
                  //    name is available, the user can always
                  //    manually re-register)
               }
               _wallet.pending_account_registrations.erase( it );
            }
         }

         return;
      }

      void _start_resync_loop()
      {
         _resync_loop_task = fc::async( [this](){ _resync_loop(); }, "cli_wallet resync loop" );
         return;
      }

      void _resync_loop()
      {
         // TODO:  Exception handling
         //    does cancel raise exception?
         _resync();
         fc::microseconds resync_interval = fc::seconds(1);

         _resync_loop_task = fc::schedule( [this](){ _resync_loop(); }, fc::time_point::now() + resync_interval, "cli_wallet resync loop" );
         return;
      }

      global_property_object get_global_properties()
      {
          return _remote_db->get_global_properties();
      }

      dynamic_global_property_object get_dynamic_global_properties()
      {
          return _remote_db->get_dynamic_global_properties();
      }

      wallet_data             _wallet;

      fc::api<login_api>      _remote_api;
      fc::api<database_api>   _remote_db;
      fc::api<network_api>    _remote_net;

      fc::future<void>        _resync_loop_task;
};

FC_API( wallet_api,
        (help)
        (list_accounts)
        (list_account_balances)
        (import_key)
        (suggest_brain_key)
        (create_account_with_brain_key)
        (transfer)
        (get_account)
        (get_account_history)
        (get_global_properties)
        (get_dynamic_global_properties)
        (get_object)
        (normalize_brain_key)
       )

struct help_visitor
{
   help_visitor( std::stringstream& s ):ss(s){}
   std::stringstream& ss;
   template<typename R, typename... Args>
   void operator()( const char* name, std::function<R(Args...)>& memb )const {
      ss << std::setw(40) << std::left << fc::get_typename<R>::name() << " " << name << "( ";
      vector<string> args{ fc::get_typename<typename std::decay<Args>::type>::name()... };
      for( uint32_t i = 0; i < args.size(); ++i )
         ss << args[i] << (i==args.size()-1?" ":", ");
      ss << ")\n";
   }

};
string  wallet_api::help()const
{
   fc::api<wallet_api> tmp;
   std::stringstream ss;
   tmp->visit( help_visitor(ss) );
   return ss.str();
}

int main( int argc, char** argv )
{
   try {
      FC_ASSERT( argc > 1, "usage: ${cmd} WALLET_FILE", ("cmd",argv[0]) );

      fc::ecc::private_key genesis_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("genesis")));
      idump( (key_to_wif( genesis_private_key ) ) );
      idump( (account_id_type()) );

      wallet_data wallet;
      fc::path wallet_file(argv[1]);
      if( fc::exists( wallet_file ) )
          wallet = fc::json::from_file( wallet_file ).as<wallet_data>();

      fc::http::websocket_client client;
      auto con  = client.connect( wallet.ws_server );
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);
      con->closed.connect( [&](){ elog( "connection closed" ); } );

      auto remote_api = apic->get_remote_api< login_api >();
      FC_ASSERT( remote_api->login( wallet.ws_user, wallet.ws_password ) );

      auto wapiptr = std::make_shared<wallet_api>(remote_api);
      wapiptr->_wallet = wallet;
      wapiptr->_start_resync_loop();

      fc::api<wallet_api> wapi(wapiptr);

      auto wallet_cli = std::make_shared<fc::rpc::cli>();
      wallet_cli->format_result( "help", [&]( variant result, const fc::variants& a) {
                                    return result.get_string();
                                });
      wallet_cli->format_result( "get_account_history", [&]( variant result, const fc::variants& a) {
                                 auto r = result.as<vector<operation_history_object>>();
                                 for( auto& i : r )
                                 {
                                    cerr << i.block_num << " "<<i.trx_in_block << " " << i.op_in_trx << " " << i.virtual_op<< " ";
                                    i.op.visit( operation_printer() );
                                    cerr << " \n";
                                 }
                                 return string();
                                });
      wallet_cli->register_api( wapi );
      wallet_cli->start();
      wallet_cli->wait();
   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
   }
   return -1;
}
