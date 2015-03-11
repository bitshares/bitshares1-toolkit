#pragma once

#include <fc/api.hpp>

namespace bts { namespace chain {


   class root_interface
   {
      public:
      /**
       *  The root API assumes it knows a shared secret derived durring
       *  the ECDH handshake.  This method will return the server's
       *  signature on that secret to prevent man in the middle attacks.
       *
       *  The user will also identify itself to the server with a key which
       *  will be used to controll access.
       */
      signature_type login( const signature_type& client_signature );

      api<database_interface> get_database_interface()const;
      api<delegate_interface> get_delegate_interface()const;
      api<archive_interface>  get_archive_interface()const;
      api<push_interface>     get_push_interface()const;


      /** used by plugins that register their own interfaces */
      uint32_t                get_interface( const string& name );
   };

   class database_interface
   {
      public:
         /**
          * This method will return an array of objects in the same order as the
          * IDS given as parameters.
          */
         vector<variant_object> get_objects( const vector<object_id_type>& ids )const;
         vector<account_object> get_accounts( const vector<string>& names )const;
         vector<asset_object>   get_assets( const vector<string>& symbols )const;
         vector<signed_block>   get_blocks( const vector<uint32_t>& block_num )const;

         /** @return the highest unfilled bid and lowest unfilled ask for the A/B market */
         pair<price,price>      get_spread( asset_id_type a, asset_id_type b )const; 

         global_property_object get_global_properties()const;
         vector<price>          get_price_feeds( asset_id_type a, asset_id_type b )const;

         block                  get_pending_block()const;
   };

   class database_subscription_interface
   {
      public:
         subscription_id_type subscribe_to_account( account_id_type accounts, const std::function<void(account_id_type)>& changed_callback );
         void                 unsubscribe_from_accounts( const vector<subscription_id_type>& subscription_ids );
   };

   /**
    *  @brief methods of use for out-of-process delegates or tran
    */
   class push_interface
   {
      public:
         void  push_transaction( const signed_transaction& trx );
         void  push_block( const signed_block& blk );
   };

   /**
    *  Things that can be accomplisehd with only the delegate signing key.
    */
   class delegate_interface
   {
      public:
         /**
          *   The next time this delegate produces a block it will uses these
          *   prices.
          */
         void publish_prices( const vector<price>& );

         /**
          *   Update the signing key.
          */
         void change_signing_key( const fc::ecc::private_key& new_signing_key );

         /**
          *  Turn block production on/off
          */
         void enable_block_production( bool on = true );
   };

   /**
    *  This interface is only supported by nodes that are doing more than validation.
    */
   class archive_interface
   {
      public:
         /**
          *  Lookup a set of transactions by ID
          */
         vector<processed_transaction>       get_transaction( const vector<transaction_id_type>& )const;

         /**
          *  Return all transactions for each account requested.
          */
         vector<vector<transaction_id_type>> get_account_transactions( const vector<account_id_type>&  )const;

         /**
          * Return all market transactions that impacted a particular set of orders.
          */
         vector<market_trxs>                 get_market_transactions( const vector<order_id_type>&  )const;

   };

   /**
    *  This interface will modify what information is archived and made available via
    *  the archive interface.   
    */
   class archive_control_interface
   {
      public:
         /** empty for all accounts */
         void add_accounts( const vector<account_id_type>& acnts );
         /** empty for all orders */
         void add_orders( const vector<order_id_type>& orders );
         /** empty for all markets */
         void add_markets( const map<asset_id_type,asset_id_type>& markets );
   };

} }



#if 0
class root_api
{
   public:
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

#endif
