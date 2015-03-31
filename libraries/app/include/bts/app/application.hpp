#pragma once

#include <bts/net/node.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace app {
   namespace detail { class application_impl; }

   class application
   {
      public:
         application(fc::path data_dir);

         typedef std::map<std::string,fc::variant> graphene_configuration;

         struct daemon_configuration
         {
            fc::ip::endpoint              p2p_endpoint;
            std::vector<fc::ip::endpoint> seed_nodes;
            fc::ip::endpoint              websocket_endpoint;
         };

         graphene_configuration&       configuration();
         const graphene_configuration& configuration()const;
         void                          apply_configuration();

         net::node_ptr                    p2p_node();
         std::shared_ptr<chain::database> chain_database()const;

      private:
         std::shared_ptr<detail::application_impl> my;
   };

} }

FC_REFLECT( bts::app::application::daemon_configuration, (p2p_endpoint)(websocket_endpoint)(seed_nodes) )
