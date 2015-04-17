#pragma once

#include <bts/app/application.hpp>

namespace bts { namespace app {

class abstract_plugin
{
   public:
      virtual ~abstract_plugin(){}
      virtual const std::string& plugin_name()const = 0;

      /** called after the database has been fully loaded */
      virtual void init(){}
};

template<class P>
class plugin : public abstract_plugin
{
   protected:
      application& app()const { return *_app; }
      chain::database& database() { return *app().chain_database(); }
      net::node& p2p_node() { return *app().p2p_node(); }

    private:
      friend class application;
      template<typename T>
      void initialize(application& app, const T& cfg )
      {
         _app = &app;
         static_cast<P*>(this)->configure(cfg);
      }
      application* _app;
};


} } //bts::app
