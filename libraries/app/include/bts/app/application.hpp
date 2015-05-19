#pragma once

#include <bts/net/node.hpp>
#include <bts/chain/database.hpp>

#include <boost/program_options.hpp>

namespace bts { namespace app {
   namespace detail { class application_impl; }
   namespace bpo = boost::program_options;
   using std::string;

   class abstract_plugin;

   class application
   {
      public:
         application();
         ~application();

         void set_program_options( bpo::options_description& command_line_options,
                                   bpo::options_description& configuration_file_options )const;
         void initialize(const fc::path& data_dir, const bpo::variables_map&options);
         void initialize_plugins( const bpo::variables_map& options );
         void startup();
         void shutdown();
         void startup_plugins();
         void shutdown_plugins();

         template<typename PluginType>
         std::shared_ptr<PluginType> register_plugin()
         {
            auto plug = std::make_shared<PluginType>();
            plug->plugin_set_app(this);

            bpo::options_description plugin_cli_options("Options for plugin " + plug->plugin_name()), plugin_cfg_options;
            plug->plugin_set_program_options(plugin_cli_options, plugin_cfg_options);
            if( !plugin_cli_options.options().empty() )
               _cli_options.add(plugin_cli_options);
            if( !plugin_cfg_options.options().empty() )
               _cfg_options.add(plugin_cfg_options);

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

         net::node_ptr                    p2p_node();
         std::shared_ptr<chain::database> chain_database()const;

         void set_block_production(bool producing_blocks);

      private:
         void add_plugin( const string& name, std::shared_ptr<abstract_plugin> p );
         std::shared_ptr<detail::application_impl> my;

         bpo::options_description _cli_options;
         bpo::options_description _cfg_options;
   };

} }
