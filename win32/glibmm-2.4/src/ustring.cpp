
#include "glibmm/ustring.h"

namespace Glib {
  ustring::ustring(std::string &str) : std::string(str) { }

  std::string &ustring::raw() { return *this; }

}
