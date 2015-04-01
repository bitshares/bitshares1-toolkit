#include <bts/app/application.hpp>

#include <bts/delegate/delegate.hpp>

#include <bts/chain/time.hpp>

#include <fc/thread/thread.hpp>
#include <fc/interprocess/signals.hpp>

using namespace bts;

int main() {
   app::application node(fc::current_path()/"delegate_node_data_dir");
   node.register_plugin<delegate_plugin::delegate_plugin>();

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
}
