#pragma once

#include <bts/app/plugin.hpp>
#include <bts/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace bts { namespace witness_plugin {
namespace bpo = boost::program_options;

class witness_plugin : public bts::app::plugin<witness_plugin> {
public:
   ~witness_plugin() {
      try {
         if( _block_production_task.valid() )
            _block_production_task.cancel_and_wait(__FUNCTION__);
      } catch(fc::canceled_exception&) {
         //Expected exception. Move along.
      } catch(fc::exception& e) {
         edump((e.to_detail_string()));
      }
   }

   const std::string& plugin_name()const override {
      static std::string name = "delegate";
      return name;
   }

   void set_program_options_impl(boost::program_options::options_description &command_line_options,
                                 boost::program_options::options_description &config_file_options);

   void set_block_production(bool allow) { _production_enabled = allow; }

   void initialize(const bpo::variables_map& options);
   void startup();

private:
   void schedule_next_production(const bts::chain::chain_parameters& global_parameters);
   void block_production_loop();

   bpo::variables_map _options;
   bool _production_enabled = false;
   std::map<chain::key_id_type, fc::ecc::private_key> _private_keys;
   std::set<chain::witness_id_type> _witnesses;
   fc::future<void> _block_production_task;
};

} } //bts::delegate
