#pragma once

#include <bts/app/application.hpp>

#include <boost/program_options.hpp>

namespace bts { namespace app {
namespace bpo = boost::program_options;

class abstract_plugin
{
   public:
      virtual ~abstract_plugin(){}
      virtual const std::string& plugin_name()const = 0;
};

template<class P>
class plugin : public abstract_plugin
{
   public:
      plugin(){}
      void set_app(application* a) { _app = a; }

      /**
       * @brief Get the program options for this plugin
       * @param command_line_options All options this plugin supports taking on the command-line
       * @param config_file_options All options this plugin supports storing in a configuration file
       *
       * Plugins MAY supply a method set_program_options_impl which populates these options_descriptions with any
       * command-line and configuration file options the plugin supports. If a plugin does not need these options, it
       * may simply omit an implementation of this method.
       */
      void set_program_options(bpo::options_description& command_line_options,
                               bpo::options_description& config_file_options)
      {
         static_cast<P*>(this)->set_program_options_impl(command_line_options, config_file_options);
      }
      /**
       * @brief Perform early startup routines and register plugin indexes, callbacks, etc.
       *
       * Plugins MUST supply a method initialize() which will be called early in the application startup. This method
       * should contain early setup code such as initializing variables, adding indexes to the database, registering
       * callback methods from the database, adding APIs, etc., as well as applying any options in the @ref options map
       *
       * This method is called BEFORE the database is open, therefore any routines which require any chain state MUST
       * NOT be called by this method. These routines should be performed in startup() instead.
       *
       * @param options The options passed to the application, via configuration files or command line
       */
      void initialize_plugin(const bpo::variables_map& options)
      {
         static_cast<P*>(this)->initialize(options);
      }
      /**
       * @brief Begin normal runtime operations
       *
       * Plugins MUST supply a method startup() which will be called at the end of application startup. This method
       * should contain code which schedules any tasks, or requires chain state.
       */
      void startup_plugin()
      {
         static_cast<P*>(this)->startup();
      }

   protected:
      chain::database& database() { return *app().chain_database(); }
      application& app()const { assert(_app); return *_app; }
      net::node& p2p_node() { return *app().p2p_node(); }

      /// Default no-op implementation of set_program_options_impl; allows plugins which have no options to elide this
      /// method
      void set_program_options_impl(boost::program_options::options_description&,
                                    boost::program_options::options_description&)const {}

    private:
      application* _app = nullptr;
};

/// @group Some useful tools for boost::program_options arguments using vectors of JSON strings
/// @{
template<typename T>
T dejsonify(const string& s)
{
   return fc::json::from_string(s).as<T>();
}

#define DEFAULT_VALUE_VECTOR(value) default_value({fc::json::to_string(value)}, fc::json::to_string(value))
#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
      const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
      std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &bts::app::dejsonify<type>); \
}
/// @}

} } //bts::app
