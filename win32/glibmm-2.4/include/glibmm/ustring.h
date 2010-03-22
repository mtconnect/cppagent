
#ifndef GLIBMM_USTRING
#define GLIBMM_USTRING

#include <libxml++config.h>

#include <string>

namespace Glib {
	class LIBXMLPP_API ustring : public std::string {
	public:
		ustring ( ) : std::string() { }
		ustring ( const ustring& str ) : std::string(str) { }
		ustring ( const ustring& str, size_t pos, size_t n = npos ) : std::string(str, pos, npos) { }
		ustring ( const char * s, size_t n ) : std::string(s, n) { }
		ustring ( const char * s ) : std::string(s) { }
		ustring ( size_t n, char c ) : std::string(n, c) { }
		ustring ( std::string &str );
		ustring (const std::string &str ) : std::string(str) {}

		template<class InputIterator> ustring (InputIterator begin, InputIterator end) : std::string(begin, end) { }

		size_type bytes() const { return length(); }
		std::string &raw();

	};
}

#endif
