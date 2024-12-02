#ifndef YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE
#define YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE

#include "segregated_storage/raw_segregated_storage.h"

#include <new>
#include <memory>
#include <type_traits>

namespace yaga {
namespace sgs {

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, size_t PageSize = DEFAULT_PAGE_SIZE>
class SegregatedStorage
{
public:
  static_assert(std::is_nothrow_destructible_v<T>, "T must have a nothrow destructor");
  struct Deleter
  {
    SegregatedStorage* allocator;
    template <typename T>
    void operator()(T* ptr) { allocator->free(ptr); }
  };
  using UPtr = std::unique_ptr<T, Deleter>;
  using SPtr = std::shared_ptr<T>;

public:
  SegregatedStorage();
  template <typename... Args>
  T* allocate(Args&&... args);
  template <typename... Args>
  SPtr allocateShared(Args&&... args);
  template <typename... Args>
  UPtr allocateUnique(Args&&... args);
  void free(T* ptr);

private:
  std::unique_ptr<IRawSegregatedStorage> rawStorage_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, size_t PageSize>
SegregatedStorage<T, PageSize>::SegregatedStorage()
{
  using RawStorage = RawSegregatedStorage<sizeof(T), alignof(T), PageSize>;
  rawStorage_ = std::make_unique<RawStorage>();
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, size_t PageSize>
template <typename... Args>
T* SegregatedStorage<T, PageSize>::allocate(Args&&... args)
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
template <typename T, size_t PageSize>
template <typename... Args>
typename SegregatedStorage<T, PageSize>::SPtr SegregatedStorage<T, PageSize>::allocateShared(Args&&... args)
{
  return SPtr(
    allocate<Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, size_t PageSize>
template <typename... Args>
typename SegregatedStorage<T, PageSize>::UPtr SegregatedStorage<T, PageSize>::allocateUnique(Args&&... args)
{
  return UPtr(
    allocate<Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, size_t PageSize>
void SegregatedStorage<T, PageSize>::free(T* ptr)
{
  std::destroy_at(ptr);
  std::byte* bytePtr = reinterpret_cast<std::byte*>(ptr);
  rawStorage_->free(bytePtr);
}

} // !namespace sgs
} // !namespace yaga

#endif // !YAGA_SEGREGATED_STORAGE_SEGREGATED_STORAGE
