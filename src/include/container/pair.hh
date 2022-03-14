#include <cstddef>
#include <utility>

#include "reservoir.hh"

#ifndef __LIBPUDDLES_OBJ_HASINIT
#define __LIBPUDDLES_OBJ_HASINIT
namespace libpuddles {
  namespace obj {

    // template<class, class = void>
    // struct hasInit : std::false_type {};

    // template< class T >
    // struct hasInit<T, std::enable_if_t<std::is_same<decltype(
    //   std::declval<T>().init(std::declval<libpuddles::reservoir_t*>(),
    //     std::declval<T>())), void>::value>>
    //   : std::true_type {};

    template <typename T>
    concept hasInit = requires(libpuddles::reservoir_t *res, T v1,
                               const T &v2) {
      v1.init(res, v2);
    };

    template <typename T>
    concept hasMoveInit = requires(libpuddles::reservoir_t *res, T v1, T v2) {
      v1.init(res, std::move(v2));
    };

    template <typename T>
    concept canMemcpyPuddles = requires(libpuddles::reservoir_t *res, T v) {
      v.memcpyPuddles();
    };
  } // namespace obj
} // namespace libpuddles

#endif

#ifndef __LIBPUDDLES_OBJ_PAIR
#define __LIBPUDDLES_OBJ_PAIR

namespace libpuddles {
  namespace obj {
    template <typename T, typename U>
    class pair {
    public:
      T first;
      U second;

      pair(libpuddles::reservoir_t *res, const T &new_first,
           const U &new_second);
      pair(libpuddles::reservoir_t *res, const T &&new_first,
           const U &&new_second);
      void init(libpuddles::reservoir_t *res, const T &new_first,
                const U &new_second);
      void init(libpuddles::reservoir_t *res, const pair<T, U> &new_pair);
      void init(libpuddles::reservoir_t *res, pair<T, U> &&new_pair);

      void memcpyPuddles() const {}
    };
  } // namespace obj
} // namespace libpuddles

using libpuddles::reservoir_t;

template <typename T, typename U>
libpuddles::obj::pair<T, U>::pair(libpuddles::reservoir_t *res,
                                  const T &new_first, const U &new_second) {
  if constexpr (hasInit<T>) {
    this->first.init(res, new_first);
  } else {
    this->first = new_first;
  }

  if constexpr (hasInit<U>) {
    this->second.init(res, new_second);
  } else {
    this->second = new_second;
  }
}

template <typename T, typename U>
libpuddles::obj::pair<T, U>::pair(libpuddles::reservoir_t *res,
                                  const T &&new_first, const U &&new_second) {
  if constexpr (std::is_trivial<T>::value || canMemcpyPuddles<T>) {
    memcpy(&this->first, &new_first, sizeof(new_first));
  } else if constexpr (hasMoveInit<T>) {
    this->first.init(res, std::forward<T>(new_first));
  } else if constexpr (hasInit<T>) {
    this->first.init(res, new_first);
  } else {
    this->first = new_first;
  }

  if constexpr (std::is_trivial<U>::value || canMemcpyPuddles<U>) {
    memcpy(&this->second, &new_second, sizeof(new_second));
  } else if constexpr (hasMoveInit<U>) {
    this->second.init(res, std::forward<U>(new_second));
  } else if constexpr (hasInit<U>) {
    this->second.init(res, new_second);
  } else {
    this->second = new_second;
  }
}

template <typename T, typename U>
void libpuddles::obj::pair<T, U>::init(libpuddles::reservoir_t *res,
                                       const T &new_first,
                                       const U &new_second) {
  if constexpr (hasInit<T>) {
    this->first.init(res, new_first);
  } else {
    this->first = new_first;
  }

  if constexpr (hasInit<U>) {
    this->second.init(res, new_second);
  } else {
    this->second = new_second;
  }
}

template <typename T, typename U>
void libpuddles::obj::pair<T, U>::init(libpuddles::reservoir_t *res,
                                       const pair<T, U> &new_pair) {
  if constexpr (hasInit<T>) {
    this->first.init(res, new_pair.first);
  } else {
    this->first = new_pair.first;
  }

  if constexpr (hasInit<U>) {
    this->second.init(res, new_pair.second);
  } else {
    this->second = new_pair.second;
  }
}

template <typename T, typename U>
void libpuddles::obj::pair<T, U>::init(libpuddles::reservoir_t *res,
                                       pair<T, U> &&new_pair) {
  if constexpr (hasMoveInit<T>) {
    this->first.init(res, std::move(new_pair.first));
  } else if constexpr (hasInit<T>) {
    this->first.init(res, new_pair.first);
  } else {
    this->first = new_pair.first;
  }

  if constexpr (hasMoveInit<U>) {
    this->second.init(res, std::move(new_pair.second));
  } else if constexpr (hasInit<U>) {
    this->second.init(res, new_pair.second);
  } else {
    this->second = new_pair.second;
  }
}

#endif
