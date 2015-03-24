#pragma once

namespace bts { namespace app {

   class application
   {
      public:
         struct config
         {
            fc::ip::endpoint p2p_endpoint;
         };

         void configure( const config& cfg );

      private:
         std::unique_ptr<application_impl>     my;
         std::shared_ptr<bts::chain::database> _chain_db;
         std::shared_ptr<bts::net::node>       _p2p_network;
   };

} }

FC_REFLECT( bts::app::application::config, (p2p_endpoint) )
