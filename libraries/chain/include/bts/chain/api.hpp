#pragma once

class root_api
{
   public:
      signature_type                 shared_secret_signature();
      map<string, api<generic_api>>  login( signature_on_shared_secret );
      map<string, api<generic_api>>  login( username, password );
};

// JSON RPC LAYER...

variant_object call( api_id, method_id, parameters )
id =  method="call" params=[ api_id, "method", 1, 2, 3 ]
   

class generic_api
{
   public:
      virtual variant call( const string& method, const vector<variant>& params )const = 0;
      virtual variant call( const string& method, vector<variant>& params ) = 0;
};

class database_rpc_api : public generic_api
{
   public:
};
class account_rpc_api : public generic_api
{
   public:
};
class wallet_rpc_api : public generic_api
{
   public:
      fc::ptr<account_rpc_api> get_account( account_id );
};



/**
 *  Info Available on CORE VALIDATION NODES
 */
class database_rpc_api
{
   public:
      vector<variant_object>  get_objects( vector<object_id_type> ids )const;
      account_object          get_account( const string& name )const;
      asset_object            get_asset( const string& symbol )const;
      signed_block            get_block( uint32_t block_num )const;

      /* return highest bid, lowest ask */
      pair<price,price>       get_spread( asset_id_type|symbol a, asset_id_type|symbol b ); 

      block                   get_pending_block( transaction_id_type )const;
      void                    push_transaction( const signed_transaction& trx );
      void                    push_block( const signed_block& blk );
};

class delegate_rpc_api
{
   public:
      void        publish_prices( vector<price> );
      void        change_key( private_key );
};

// DYNAMIC SUBSET OF USERS
class archive_rpc_api
{
   public:
      /** index trxid->block/trxnum  read block, return trx num */
      processed_transaction   get_transaction( transaction_id_type )const;
      vector<trx_id>          get_account_transactions( account_id_type )const;
      market_trxs             get_market_transactions( order_id_type )const;

};

class market_rpc_api
{
   public:
      // ALL USERS
      vector<market_status>  get_market_history( start_date, end_date, interval ); // bid,ask,volume,open,close
      market_trxs            get_market_transactions( start_date, end_date );
};


class account_api
{
    string              get_memo( account_id, transaction_id );
    signed_transaction  sign_transaction( signed_transaction );

    void                start_transaction();
    operation           add_operation( operation ) // calculates fees and returns actual operation added
    signed_transaction  current_transaction();
    signed_transaction  sign_transaction( signed_transaction );
};

/**
 *  Wallet has Private Keys 
 */
class wallet_rpc_api
{
    open();
    close();
    lock();
    unlock();

    account_api_ptr   register_account(....) // may be partially signed
    account_api_ptr   get_account( name )

};


ptr<database_rpc_api>   api;

