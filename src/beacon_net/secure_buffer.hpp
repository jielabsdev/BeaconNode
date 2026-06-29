#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

#ifdef BEACONNODE_ENABLE_LIBSODIUM
#include <sodium.h>
#endif

namespace beacon {

class SecureBuffer {
 public:
  SecureBuffer() noexcept = default;

  explicit SecureBuffer(std::size_t size) {
    if (size > 0) alloc(size);
  }

  ~SecureBuffer() { wipe_and_free(); }

  SecureBuffer(const SecureBuffer&) = delete;
  SecureBuffer& operator=(const SecureBuffer&) = delete;

  SecureBuffer(SecureBuffer&& other) noexcept
      : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  SecureBuffer& operator=(SecureBuffer&& other) noexcept {
    if (this != &other) {
      wipe_and_free();
      data_ = other.data_;
      size_ = other.size_;
      other.data_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  std::uint8_t* data() noexcept { return data_; }
  const std::uint8_t* data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }
  explicit operator bool() const noexcept { return data_ != nullptr; }

  bool constant_time_equals(const SecureBuffer& other) const noexcept {
    if (size_ != other.size_) return false;
    if (!data_ || !other.data_) return data_ == other.data_;
#ifdef BEACONNODE_ENABLE_LIBSODIUM
    return sodium_memcmp(data_, other.data_, size_) == 0;
#else
    volatile std::uint8_t diff = 0;
    for (std::size_t i = 0; i < size_; ++i) {
      diff |= (data_[i] ^ other.data_[i]);
    }
    return diff == 0;
#endif
  }

  void resize(std::size_t new_size) {
    if (new_size == size_ || new_size == 0) {
      if (new_size == 0) clear();
      return;
    }
    auto* old = data_;
    auto old_size = size_;
    alloc(new_size);
    if (old && data_) {
      std::memcpy(data_, old, old_size < new_size ? old_size : new_size);
    }
    if (old) {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
      sodium_memzero(old, old_size);
      sodium_free(old);
#else
      if (old_size > 0) {
        volatile std::uint8_t* p = static_cast<volatile std::uint8_t*>(old);
        for (std::size_t i = 0; i < old_size; ++i) p[i] = 0;
      }
      std::free(old);
#endif
    }
  }

  void clear() noexcept {
    wipe_and_free();
    data_ = nullptr;
    size_ = 0;
  }

  std::uint8_t& operator[](std::size_t i) { return data_[i]; }
  const std::uint8_t& operator[](std::size_t i) const { return data_[i]; }

  std::uint8_t* begin() noexcept { return data_; }
  std::uint8_t* end() noexcept { return data_ ? data_ + size_ : nullptr; }
  const std::uint8_t* begin() const noexcept { return data_; }
  const std::uint8_t* end() const noexcept {
    return data_ ? data_ + size_ : nullptr;
  }

 private:
  void alloc(std::size_t size) {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
    data_ = static_cast<std::uint8_t*>(sodium_malloc(size));
    if (!data_) throw std::bad_alloc();
    size_ = size;
    sodium_mlock(data_, size_);
#else
    data_ = static_cast<std::uint8_t*>(std::malloc(size));
    if (!data_) throw std::bad_alloc();
    size_ = size;
#endif
  }

  void wipe_and_free() noexcept {
    if (!data_) return;
#ifdef BEACONNODE_ENABLE_LIBSODIUM
    sodium_munlock(data_, size_);
    sodium_memzero(data_, size_);
    sodium_free(data_);
#else
    volatile std::uint8_t* p = static_cast<volatile std::uint8_t*>(data_);
    for (std::size_t i = 0; i < size_; ++i) p[i] = 0;
    std::free(data_);
#endif
  }

  std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
};

template <typename T>
struct SecureAllocator {
  using value_type = T;

  SecureAllocator() = default;
  template <typename U>
  SecureAllocator(const SecureAllocator<U>&) {}

  T* allocate(std::size_t n) {
#ifdef BEACONNODE_ENABLE_LIBSODIUM
    auto* ptr = static_cast<T*>(sodium_malloc(n * sizeof(T)));
    if (ptr) sodium_mlock(ptr, n * sizeof(T));
    return ptr;
#else
    return static_cast<T*>(std::malloc(n * sizeof(T)));
#endif
  }

  void deallocate(T* p, std::size_t n) noexcept {
    if (!p) return;
#ifdef BEACONNODE_ENABLE_LIBSODIUM
    sodium_memzero(p, n * sizeof(T));
    sodium_munlock(p, n * sizeof(T));
    sodium_free(p);
#else
    volatile std::uint8_t* v = static_cast<volatile std::uint8_t*>(p);
    for (std::size_t i = 0; i < n * sizeof(T); ++i) v[i] = 0;
    std::free(p);
#endif
  }
};

template <typename T, typename U>
bool operator==(const SecureAllocator<T>&, const SecureAllocator<U>&) {
  return true;
}
template <typename T, typename U>
bool operator!=(const SecureAllocator<T>&, const SecureAllocator<U>&) {
  return false;
}

}  // namespace beacon
