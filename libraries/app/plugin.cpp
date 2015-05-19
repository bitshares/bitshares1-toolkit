
#include <bts/app/plugin.hpp>

namespace bts { namespace app {

plugin::plugin()
{
   _app = nullptr;
   return;
}

plugin::~plugin()
{
   return;
}

std::string plugin::plugin_name()const
{
   return "<unknown plugin>";
}

void plugin::plugin_initialize( const bpo::variables_map& options )
{
   return;
}

void plugin::plugin_startup()
{
   return;
}

void plugin::plugin_shutdown()
{
   return;
}

void plugin::plugin_set_app( application* app )
{
   _app = app;
   return;
}

void plugin::plugin_set_program_options(
   bpo::options_description& command_line_options,
   bpo::options_description& config_file_options
)
{
   return;
}

} } // bts::app
