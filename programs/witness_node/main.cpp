#include <bts/app/application.hpp>

#include <bts/witness/witness.hpp>
#include <bts/account_history/account_history_plugin.hpp>

#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>

#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>

#ifndef WIN32
#include <csignal>
#endif

using namespace bts;
namespace bpo = boost::program_options;

int main(int argc, char** argv) {
   try {
      app::application node;
      bpo::options_description app_options("Graphene Witness Node");
      bpo::options_description cfg_options("Graphene Witness Node");
      app_options.add_options()
            ("help,h", "Print this help message and exit.")
            ("data-dir,d", bpo::value<boost::filesystem::path>()->default_value("witness_node_data_dir"), "Directory containing databases, configuration file, etc.")
            ;

      bpo::variables_map options;

      auto witness_plug = node.register_plugin<witness_plugin::witness_plugin>();
      auto history_plug = node.register_plugin<account_history::account_history_plugin>();

      {
         bpo::options_description cli, cfg;
         node.set_program_options(cli, cfg);
         app_options.add(cli);
         cfg_options.add(cfg);
         bpo::store(bpo::parse_command_line(argc, argv, app_options), options);
      }

      if( options.count("help") )
      {
         std::cout << app_options << "\n";
         return 0;
      }

      fc::path data_dir;
      if( options.count("data-dir") )
      {
         data_dir = options["data-dir"].as<boost::filesystem::path>();
         if( data_dir.is_relative() )
            data_dir = fc::current_path() / data_dir;
      }

      if( fc::exists(data_dir / "config.ini") )
         bpo::store(bpo::parse_config_file<char>((data_dir / "config.ini").preferred_string().c_str(), cfg_options), options);
      else {
         ilog("Writing new config file at ${path}", ("path", data_dir/"config.ini"));
         if( !fc::exists(data_dir) )
            fc::create_directories(data_dir);

         std::ofstream out_cfg((data_dir / "config.ini").preferred_string());
         for( const boost::shared_ptr<bpo::option_description> od : cfg_options.options() )
         {
            if( !od->description().empty() )
               out_cfg << "# " << od->description() << "\n";
            boost::any store;
            if( !od->semantic()->apply_default(store) )
               out_cfg << "# " << od->long_name() << " = \n";
            else {
               auto example = od->format_parameter();
               if( example.empty() )
                  // This is a boolean switch
                  out_cfg << od->long_name() << " = " << "false\n";
               else {
                  // The string is formatted "arg (=<interesting part>)"
                  example.erase(0, 6);
                  example.erase(example.length()-1);
                  out_cfg << od->long_name() << " = " << example << "\n";
               }
            }
            out_cfg << "\n";
         }
      }

      bpo::notify(options);
      node.initialize(data_dir, options);
      node.initialize_plugins( options );

      node.startup();
      node.startup_plugins();

      fc::promise<int>::ptr exit_promise = new fc::promise<int>("UNIX Signal Handler");
#ifdef __unix__
      fc::set_signal_handler([&exit_promise](int signal) {
         exit_promise->set_value(signal);
      }, SIGINT);
#endif

      ilog("Started witness node on a chain with ${h} blocks.", ("h", node.chain_database()->head_block_num()));

      int signal = exit_promise->wait();
      ilog("Exiting from signal ${n}", ("n", signal));
      node.shutdown_plugins();
      return 0;
   } catch( const fc::exception& e ) {
      elog("Exiting with error:\n${e}", ("e", e.to_detail_string()));
      return 1;
   }
}
