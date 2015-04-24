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
         application();
         ~application();

         typedef std::map<string,fc::variant> config;

         struct daemon_configuration
         {
            fc::ip::endpoint              p2p_endpoint;
            std::vector<fc::ip::endpoint> seed_nodes;
            fc::ip::endpoint              websocket_endpoint = fc::ip::endpoint::from_string("127.0.0.1:8090");
            chain::genesis_allocation     initial_allocation = {
               {bts::chain::public_key_type(fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan"))).get_public_key()), 1}
            };
         };

         void configure( const fc::path& data_dir );
         void configure( const fc::path& data_dir, const daemon_configuration& config );

         template<typename PluginType>
         std::shared_ptr<PluginType> register_plugin()
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

            return plug;
         }
         std::shared_ptr<abstract_plugin> get_plugin( const string& name )const;

         template<typename PluginType>
         std::shared_ptr<PluginType> get_plugin( const string& name ) const
         {
            std::shared_ptr<abstract_plugin> abs_plugin = get_plugin( name );
            std::shared_ptr<PluginType> result = std::dynamic_pointer_cast<PluginType>( abs_plugin );
            FC_ASSERT( result != std::shared_ptr<PluginType>() );
            return result;
         }

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

FC_REFLECT( bts::app::application::daemon_configuration,
            (p2p_endpoint)(websocket_endpoint)(seed_nodes)(initial_allocation) )
