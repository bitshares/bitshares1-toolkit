#include <bts/app/application.hpp>

#include <bts/delegate/delegate.hpp>

#include <fc/thread/thread.hpp>

using namespace bts;

int main() {
   app::application node(fc::current_path()/"delegate_node_data_dir");
   node.register_plugin<delegate_plugin::delegate_plugin>();

   ilog("Started delegate node on a chain with ${h} blocks.", ("h", node.chain_database()->head_block_num()));

   fc::usleep(fc::days(100));
}
