#include <cstddef>
#include <iostream>
#include <string>
#include <cstring>

#include "container/vector.hh"
#include "container/reservoir.hh"

#ifndef __LIBPUDDLES_OBJ_STRING
#define __LIBPUDDLES_OBJ_STRING

namespace libpuddles {
  namespace obj {
    /**
     * @brief String class using libpuddles
     *
     * @details Supports SSO. Last byte in SSO mode is for the remaining
     * capacity, works as NULL terminator if the string is zero capacity
     */
    class string {
      /** Maximum number of characters supported in SSO mode */
      static constexpr size_t SSO_SZ = 24 - 1;
      union content {
        struct {
          /** The first 64 bits of the vector is the size of the vector */
          libpuddles::obj::vector<char> content;
        } non_sso;
        struct {
          uint64_t sz : 63;
          uint64_t is_sso_used : 1; /**< MSB of the sz field */
          char content[SSO_SZ];
          char null_term;
        } sso __attribute__((packed));
        content() {}
      } content;
      void sso_init(const std::string &str);
      void sso_init(const obj::string &str);

    public:
      char *c_str() const {
        if (is_sso_used()) {
          return (char *)this->content.sso.content;
        } else {
          return this->content.non_sso.content.raw_data();
        }
      }

      bool is_sso_used() const { return this->content.sso.is_sso_used; }
      string(){};
      string(reservoir_t *res, const std::string &str);
      void init(libpuddles::reservoir_t *res, const std::string &str);
      void init(libpuddles::reservoir_t *res,
                const libpuddles::obj::string &str);
      void init(libpuddles::reservoir_t *res, libpuddles::obj::string &&str);

      bool operator==(const std::string &str) const;
      size_t size() const {
        return this->content.sso.sz & ~(1UL<<63);
      }

      void memcpyPuddles() const {}

    };
  } // namespace obj
} // namespace libpuddles

/* === implementation === */

namespace libpuddles {
  inline bool
  libpuddles::obj::string::operator==(const std::string &str) const {
    if (this->is_sso_used()) {
      const auto str_sz = str.size();
      if (str_sz != this->size()) {
        return false;
      } else {
        const char *memcmp_arg0 = str.c_str();
        const char *memcmp_arg1 = &this->content.sso.content[0];
        // return memcmp(memcmp_arg0, memcmp_arg1, str.size()) == 0;
        return 0 == std::char_traits<char>::compare(memcmp_arg0, memcmp_arg1, str_sz);
      }
    } else {
      if (str.size() != this->size()) return false;

      const char *memcmp_arg0 = str.c_str();
      const char *memcmp_arg1 = this->content.non_sso.content.raw_data();
      return std::char_traits<char>::compare(memcmp_arg0, memcmp_arg1, str.size()) == 0;
    }
  }
  inline void obj::string::sso_init(const std::string &str) {

    memcpy(&this->content.sso.content[0], str.c_str(), str.size() + 1);
    this->content.sso.is_sso_used = 1;
  }

  inline void obj::string::sso_init(const obj::string &str) {
    memcpy(this, &str, sizeof(*this));
    // libcommon::pmemops->flush(this, sizeof(*this));
    this->content.sso.is_sso_used = 1;
  }

  inline obj::string::string(reservoir_t *res, const std::string &str) {
    this->init(res, str);
  }

  inline void obj::string::init(reservoir_t *res, const std::string &str) {
    this->content.sso.sz = str.size();
    if (str.size() <= SSO_SZ) {
      this->sso_init(str);
    } else {
      this->content.non_sso.content.init(res, str.c_str(), str.size() + 1);
      this->content.sso.is_sso_used = 0;
    }
  }

  inline void obj::string::init(reservoir_t *res, const obj::string &str) {
    this->content.sso.sz = str.size();
    if (str.size() <= SSO_SZ) {
      sso_init(str);
    } else {
      this->content.non_sso.content.init(res, str.content.non_sso.content);
      this->content.sso.is_sso_used = 0;
    }
  }

  inline void obj::string::init(reservoir_t *res, obj::string &&str) {
    this->content.sso.sz = str.size();
    if (str.size() <= SSO_SZ) {
      this->sso_init(str);
    } else {
      auto &ns_content = this->content.non_sso.content;
      ns_content.init(res, std::move(str.content.non_sso.content));
      this->content.sso.is_sso_used = 0;
    }
  }
} // namespace libpuddles
inline std::ostream &operator<<(std::ostream &o,
                                const libpuddles::obj::string &str) {
  o << std::string(str.c_str());
  return o;
}
#endif
