#ifndef YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE
#define YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE

#include "segregated_storage/raw_segregated_storage.h"

#include <new>
#include <memory>
#include <type_traits>

namespace yaga {
namespace sgs {

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
class SegregatedStorage
{
public:
  static_assert(std::is_nothrow_destructible_v<T>, "T must have a nothrow destructor");
  struct Deleter
  {
    SegregatedStorage* allocator;
    template <typename P>
    void operator()(P* ptr) { allocator->free(ptr); }
  };
  using UPtr = std::unique_ptr<T, Deleter>;
  using SPtr = std::shared_ptr<T>;

public:
  explicit SegregatedStorage(size_t pageSize = DEFAULT_PAGE_SIZE);
  template <typename... Args>
  T* allocate(Args&&... args);
  template <typename... Args>
  SPtr allocateShared(Args&&... args);
  template <typename... Args>
  UPtr allocateUnique(Args&&... args);
  void free(T* ptr);

private:
  using RawStorage = RawSegregatedStorage<sizeof(T), alignof(T)>;
  std::unique_ptr<RawStorage> rawStorage_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
SegregatedStorage<T>::SegregatedStorage(size_t pageSize)
{
  using RawStorage = RawSegregatedStorage<sizeof(T), alignof(T)>;
  rawStorage_ = std::make_unique<RawStorage>(pageSize);
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
template <typename... Args>
T* SegregatedStorage<T>::allocate(Args&&... args)
{
  std::byte* bytePtr = rawStorage_->allocate();
  try {
    return ::new(bytePtr) T(std::forward<Args>(args)...);
  }
  catch (...) {
    rawStorage_->free(bytePtr);
    throw;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
template <typename... Args>
typename SegregatedStorage<T>::SPtr SegregatedStorage<T>::allocateShared(Args&&... args)
{
  return SPtr(
    allocate<Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
template <typename... Args>
typename SegregatedStorage<T>::UPtr SegregatedStorage<T>::allocateUnique(Args&&... args)
{
  return UPtr(
    allocate<Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
void SegregatedStorage<T>::free(T* ptr)
{
  std::destroy_at(ptr);
  std::byte* bytePtr = reinterpret_cast<std::byte*>(ptr);
  rawStorage_->free(bytePtr);
}

} // !namespace sgs
} // !namespace yaga

#endif // !YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE
