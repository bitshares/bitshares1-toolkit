#pragma once

#include <bts/net/node.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace app {
   namespace detail { class application_impl; }

   class application
   {
      public:
         application(fc::path data_dir);

         struct config
         {
            fc::ip::endpoint p2p_endpoint;
            std::vector<fc::ip::endpoint> seed_nodes;
         };

         void configure( const config& cfg );

         net::node_ptr p2p_node();
         std::shared_ptr<chain::database> chain_database()const;

      private:
         std::shared_ptr<detail::application_impl> my;
   };

} }

FC_REFLECT( bts::app::application::config, (p2p_endpoint)(seed_nodes) )
