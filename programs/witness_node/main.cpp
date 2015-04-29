#include <bts/app/application.hpp>

#include <bts/witness/witness.hpp>
#include <bts/account_history/account_history_plugin.hpp>

#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>

#ifndef WIN32
#include <csignal>
#endif

using namespace bts;

int main(int argc, char** argv) {
   try {
      app::application node;
      node.configure(fc::current_path()/"witness_node_data_dir");
      auto witness_plug = node.register_plugin<witness_plugin::witness_plugin>();
      auto history_plug = node.register_plugin<account_history::account_history_plugin>();
      node.init();
      if( argc > 1 )
         witness_plug->set_block_production(true);

      //Start NTP time client
      chain::now();

      fc::promise<int>::ptr exit_promise = new fc::promise<int>("UNIX Signal Handler");
      fc::set_signal_handler([&exit_promise](int signal) {
         exit_promise->set_value(signal);
      }, SIGINT);

      ilog("Started witness node on a chain with ${h} blocks.", ("h", node.chain_database()->head_block_num()));

      int signal = exit_promise->wait();
      ilog("Exiting from signal ${n}", ("n", signal));
      chain::shutdown_ntp_time();
      return 0;
   } catch( const fc::exception& e ) {
      elog("Exiting with error:\n${e}", ("e", e.to_detail_string()));
      return 1;
   }
}
