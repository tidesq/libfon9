/// \file fon9/seed/SeedBase.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/SeedBase.hpp"

namespace fon9 { namespace seed {

fon9_API const char* GetOpResultMessage(OpResult res) {
#define case_return(r)  case OpResult::r:    return #r;
   switch (res) {
      case_return(no_error);
      case_return(removed_pod);
      case_return(removed_seed);

      case_return(access_denied);
      case_return(mem_alloc_error);

      case_return(key_exists);
      case_return(key_format_error);
      case_return(value_format_error);
      case_return(value_overflow);
      case_return(value_underflow);

      case_return(bad_command_argument);

      case_return(not_supported_cmd);
      case_return(not_supported_read);
      case_return(not_supported_write);
      case_return(not_supported_null);
      case_return(not_supported_number);

      case_return(not_supported_get_seed);
      case_return(not_supported_get_pod);
      case_return(not_supported_add_pod);
      case_return(not_supported_remove_pod);
      case_return(not_supported_remove_seed);
      case_return(not_supported_grid_view);
      case_return(not_supported_tree_op);

      case_return(not_found_key);
      case_return(not_found_tab);
      case_return(not_found_seed);
      case_return(not_found_sapling);
      case_return(not_found_field);
   }
   return "Unknown OpResult";
}

} } // namespaces
