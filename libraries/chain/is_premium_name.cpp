#include <unordered_set> 
#include <string> 
#include <vector> 
#include <fc/io/json.hpp>

namespace bts { namespace chain {

bool is_premium_name( const std::string& n )
{
   std::unordered_set<std::string> premium_names = []()
   {
      std::unordered_set<std::string> result;
      std::vector<std::string> words = {
#include "words.json"
         };
      for( auto item : words )
      {
         auto vec = fc::json::from_string(item).as<std::vector<std::string>>();
         for( auto word : vec )
            result.insert(word);
      }
      return result;
   }();

   return false;//words.find(n) != words.end();
}
} }
