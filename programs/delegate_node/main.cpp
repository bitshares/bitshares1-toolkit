#include <bts/app/application.hpp>

#include <bts/delegate/delegate.hpp>

#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>

using namespace bts;

int main(int argc, char** argv) {
   try {
      app::application node(fc::current_path()/"delegate_node_data_dir");
      auto delegate_plug = node.register_plugin<delegate_plugin::delegate_plugin>();
      if( argc > 1 )
         delegate_plug->set_block_production(true);

      //Start NTP time client
      chain::now();

      fc::promise<int>::ptr exit_promise = new fc::promise<int>("UNIX Signal Handler");
      fc::set_signal_handler([&exit_promise](int signal) {
         exit_promise->set_value(signal);
      }, SIGINT);

      ilog("Started delegate node on a chain with ${h} blocks.", ("h", node.chain_database()->head_block_num()));

      int signal = exit_promise->wait();
      ilog("Exiting from signal ${n}", ("n", signal));
      chain::shutdown_ntp_time();
      return 0;
   } catch( const fc::exception& e ) {
      elog("Exiting with error:\n${e}", ("e", e.to_detail_string()));
      return 1;
   }
}
