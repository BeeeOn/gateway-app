#ifndef COMPAT_H
#define COMPAT_H

#include <string>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <climits>
#include <cerrno>

#ifndef HAVE_CXX_TO_STRING
namespace std {
  template <typename T> inline std::string to_string(const T& n) {
    std::ostringstream stm;
    stm << n;
    return stm.str();
  }
}
#endif

#ifndef HAVE_CXX_STOI
namespace std {
  inline int stoi(const std::string& str, size_t *idx = 0, int base = 10) {
    char *endp;
    int val = static_cast<int>(strtol(str.c_str(), &endp, base));
    if(endp == str.c_str()) {
      throw std::invalid_argument("stoi");
    }
    if(val == LONG_MAX && errno == ERANGE) {
      throw std::out_of_range("stoi");
    }
    if(idx) {
      *idx = endp - str.c_str();
    }

    return val;
  }
}
#endif

#ifndef HAVE_CXX_STOUL
namespace std {
  inline unsigned long stoul(const std::string& str, size_t *idx = 0, int base = 10) {
    char *endp;
    unsigned long val = strtoul(str.c_str(), &endp, base);
    if(endp == str.c_str()) {
      throw std::invalid_argument("strtoul");
    }
    if(val == ULONG_MAX && errno == ERANGE) {
      throw std::out_of_range("strtoul");
    }
    if(idx) {
      *idx = endp - str.c_str();
    }

    return val;
  }
}
#endif

#ifndef HAVE_CXX_STOULL
namespace std {
  inline unsigned long long stoull(const std::string& str, size_t *idx = 0, int base = 10) {
    char *endp;
    unsigned long long val = strtoull(str.c_str(), &endp, base);
    if(endp == str.c_str()) {
      throw std::invalid_argument("strtoull");
    }
    if(val == ULLONG_MAX && errno == ERANGE) {
      throw std::out_of_range("strtoull");
    }
    if(idx) {
      *idx = endp - str.c_str();
    }

    return val;
  }
}
#endif

#ifndef HAVE_CXX_STOF
namespace std {
  inline float stof(const std::string& str) {
    return atof(str.c_str());
  }
}
#endif

#endif /* COMPAT_H */
