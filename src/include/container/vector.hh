#include <cstddef>

#include "container/reservoir.hh"
#include "libstoreinst.hh"

#include <cassert>
#include <utility>
#include <iostream>

#ifndef __LIBPUDDLES_OBJ_HASINIT
#define __LIBPUDDLES_OBJ_HASINIT
namespace libpuddles {
  namespace obj {

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

#ifndef __LIBPUDDLES_OBJ_VECTOR
#define __LIBPUDDLES_OBJ_VECTOR

namespace libpuddles {
  namespace obj {
    template <typename T>
    class vector {
      size_t _size;
      T *content;
      size_t capacity;

    public:
      vector();
      void init(reservoir_t *res);
      void init(reservoir_t *res, size_t num);
      void init(reservoir_t *res, const T *new_elem, size_t num);
      void init(reservoir_t *res, const libpuddles::obj::vector<T> &new_elems);
      void init(reservoir_t *res, libpuddles::obj::vector<T> &&new_elems);
      void push_back(reservoir_t *res, const T &new_elem);
      void push_back(reservoir_t *res, T &&new_elem);
      void resize(reservoir_t *res);
      inline void clear();
      void set_at(reservoir_t *res, const T &new_elem, size_t idx);
      inline size_t size() const;
      T *raw_data() const;

      T &operator[](const size_t idx) const;
    };
  } // namespace obj
} // namespace libpuddles

using libpuddles::reservoir_t;

template <typename T>
libpuddles::obj::vector<T>::vector() {
  // TX_ADD(this);
  this->content = nullptr;
  this->capacity = 0;
  this->_size = 0;
  // libcommon::pmemops->flush(this, sizeof(*this));

  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::init(reservoir_t *res) {
  // TX_ADD(this);
  this->content = res->malloc<T>(2);
  this->capacity = 2;
  this->_size = 0;
  // libcommon::pmemops->flush(this, sizeof(*this));
  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::resize(reservoir_t *res) {
  // TX_ADD(this);

  size_t new_capacity = this->capacity == 0 ? 2 : this->capacity * 2;
  auto tmp_content = res->malloc<T>(new_capacity);

  // std::cout << "new capacity = " << P(tmp_content) << " cache aligned = "
  // << (void*)align_cl(P(tmp_content)) << std::endl;

  if constexpr (std::is_trivial<T>::value || canMemcpyPuddles<T>) {
    const auto buf_sz = sizeof(this->content[0]) * this->capacity;
    memcpy((void *)tmp_content, (void *)this->content, buf_sz);
    // libcommon::pmemops->flush(tmp_content, buf_sz);
  } else {
    for (size_t i = 0; i < this->capacity; ++i) {
      if constexpr (hasMoveInit<T>) {
        tmp_content[i].init(res, std::move(this->content[i]));
      } else if constexpr (hasInit<T>) {
        tmp_content[i].init(res, this->content[i]);
      } else {
        tmp_content[i] = this->content[i];
        // libcommon::pmemops->flush(&tmp_content[i], sizeof(tmp_content[i]));
      }
    }
  }

  if (this->content) res->free(this->content);
  this->content = tmp_content;
  this->capacity = new_capacity;
  // libcommon::pmemops->flush(this, sizeof(*this));
}

template <typename T>
void libpuddles::obj::vector<T>::push_back(reservoir_t *res,
                                           const T &new_elem) {
  if (this->_size == this->capacity) {
    this->resize(res);
  } else {
    // TX_ADD(&this->_size);
  }

  if constexpr (hasInit<T>) {
    this->content[_size++].init(res, new_elem);
  } else {
    this->content[_size] = new_elem;
    // libcommon::pmemops->flush(&this->content[_size],
                              // sizeof(this->content[_size]));
    _size++;
  }

  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::push_back(reservoir_t *res, T &&new_elem) {
  if (this->_size == this->capacity) {
    this->resize(res);
  } else {
    // TX_ADD(&this->_size);
  }

  // if constexpr (std::is_trivial<T>::value || canMemcpyPuddles<T>) {
  //   memcpy((void*)&this->content[_size++], (void*)&new_elem,
  //   sizeof(this->content[0]));
  // }
  // else {
  assert(hasMoveInit<T> && "No move init!");
  this->content[_size++].init(res, std::forward<T>(new_elem));
  // }
  // libcommon::pmemops->flush(&this->content[_size - 1],
                            // sizeof(this->content[_size - 1]));

  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::set_at(reservoir_t *res, const T &new_elem,
                                        size_t idx) {
  // TODO: idx check
  // TX_ADD(&this->content[idx]);
  this->content[idx] = new_elem;
  // libcommon::pmemops->flush(&this->content[idx], sizeof(this->content[idx]));

  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::init(reservoir_t *res, const T *elems,
                                      size_t num) {
  this->content = res->malloc<T>(num * 2);
  // std::cout << "this->conent = " << (void*)(this->content)
  //           << " cache aligned = " << (void *)align_cl(this->content)
  //           << std::endl;
  this->capacity = num * 2;
  this->_size = num;

  if constexpr (std::is_trivial<T>::value) {
    const auto buf_sz = sizeof(this->content[0]) * num;
    memcpy((void *)this->content, (void *)elems, buf_sz);
    // libcommon::pmemops->flush(this->content, buf_sz);
  } else {
    // for (size_t i = 0; i < elems._size; ++i) {
    //   if constexpr (hasInit<T>) {
    //     this->content[i].init(res, elems.content[i]);
    //   } else {
    //     this->content[i] = elems.content[i];
    //     // libcommon::pmemops->flush(&this->content[i], sizeof(this->content[0]));
    //   }
    // }
  }

  // libcommon::pmemops->flush(this, sizeof(*this));
}

template <typename T>
void libpuddles::obj::vector<T>::init(
    reservoir_t *res, const libpuddles::obj::vector<T> &new_elems) {
  // TX_ADD(this);
  this->content = res->malloc<T>(new_elems.capacity);
  this->capacity = new_elems.capacity;
  this->_size = new_elems._size;

  if constexpr (std::is_trivial<T>::value) {
    const auto buf_sz = sizeof(this->content[0]) * this->_size;
    memcpy((void *)this->content, (void *)new_elems.content, buf_sz);
    // libcommon::pmemops->flush(this->content, buf_sz);
  } else {
    for (size_t i = 0; i < new_elems._size; ++i) {
      if constexpr (hasInit<T>) {
        this->content[i].init(res, new_elems.content[i]);
      } else {
        this->content[i] = new_elems.content[i];
        // libcommon::pmemops->flush(&this->content[i], sizeof(this->content[0]));
      }
    }
  }

  // libcommon::pmemops->flush(this, sizeof(*this));
  // TODO: flush (and fence?)
}

template <typename T>
void libpuddles::obj::vector<T>::init(reservoir_t *res,
                                      libpuddles::obj::vector<T> &&new_elems) {
  (void)res;

  this->content = new_elems.content;
  this->capacity = new_elems.capacity;
  this->_size = new_elems._size;

  // TX_ADD(&new_elems);
  new_elems.content = nullptr;
  new_elems.capacity = 0;
  new_elems._size = 0;

  // libcommon::pmemops->flush(&new_elems, sizeof(new_elems));

  // libcommon::pmemops->flush(this, sizeof(*this));
}

template <typename T>
inline size_t libpuddles::obj::vector<T>::size() const {
  return this->_size;
}

template <typename T>
T &libpuddles::obj::vector<T>::operator[](const size_t idx) const {
  return this->content[idx];
}

template <typename T>
T *libpuddles::obj::vector<T>::raw_data() const {
  return this->content;
}

#endif
