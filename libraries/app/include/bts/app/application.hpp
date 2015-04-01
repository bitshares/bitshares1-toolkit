#pragma once

#include <bts/net/node.hpp>
#include <bts/chain/database.hpp>

namespace bts { namespace app {
   namespace detail { class application_impl; }
   using std::string;

   class abstract_plugin;

   class application
   {
      public:
         application(fc::path data_dir);
         ~application();

         typedef std::map<string,fc::variant> config;

         struct daemon_configuration
         {
            fc::ip::endpoint              p2p_endpoint;
            std::vector<fc::ip::endpoint> seed_nodes;
            fc::ip::endpoint              websocket_endpoint;
         };

         template<typename PluginType>
         void register_plugin()
         {
            auto plug = std::make_shared<PluginType>();
            typename PluginType::plugin_config cfg;
            try {
               cfg = configuration()[plug->plugin_name()].template as<decltype(cfg)>();
            } catch(fc::exception& e) {
               ilog("Initializing new configuration for '${name}' plugin.", ("name", plug->plugin_name()));
               configuration()[plug->plugin_name()] = cfg;
               save_configuration();
            }
            plug->initialize( *this, cfg);
            add_plugin( plug->plugin_name(), plug );
         }
         std::shared_ptr<abstract_plugin> get_plugin( const string& name )const;

         config&        configuration();
         const config&  configuration()const;
         void           apply_configuration();
         void           save_configuration()const;

         net::node_ptr                    p2p_node();
         std::shared_ptr<chain::database> chain_database()const;

      private:
         void add_plugin( const string& name, std::shared_ptr<abstract_plugin> p );
         std::shared_ptr<detail::application_impl> my;
   };

} }

FC_REFLECT( bts::app::application::daemon_configuration, (p2p_endpoint)(websocket_endpoint)(seed_nodes) )
