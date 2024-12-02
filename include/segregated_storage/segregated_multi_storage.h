#ifndef YAGA_SEGREGATED_STORAGE_SEGREGATED_MULTI_STORAGE
#define YAGA_SEGREGATED_STORAGE_SEGREGATED_MULTI_STORAGE

#include "segregated_storage/raw_segregated_storage.h"

#include <new>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace yaga {
namespace sgs {

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize = DEFAULT_PAGE_SIZE>
class SegregatedMultiStorage
{
public:
  struct Deleter
  {
    SegregatedMultiStorage* allocator;
    template <typename T>
    void operator()(T* ptr) { allocator->free(ptr); }
  };
  template <typename T>
  struct Config
  {
    static const size_t pageSize = PageSize;
  };  
  template <typename T>
  using UPtr = std::unique_ptr<T, Deleter>;
  template <typename T>
  using SPtr = std::shared_ptr<T>;

public:
  template <typename T, typename... Args>
  T* allocate(Args&&... args);
  template <typename T, typename... Args>
  SPtr<T> allocateShared(Args&&... args);
  template <typename T, typename... Args>
  UPtr<T> allocateUnique(Args&&... args);
  template <typename T>
  void free(T* ptr);

private:
  using Key = std::pair<size_t, size_t>;
  using StoragePtr = std::unique_ptr<IRawSegregatedStorage>;
  struct Hash
  {
    size_t operator()(const Key& key) const;
  };

private:
  template <typename T>
  IRawSegregatedStorage* findStorage();
  template <typename T>
  IRawSegregatedStorage* createStorage();

private:
  std::unordered_map<Key, StoragePtr, Hash> storage_;
  std::shared_mutex mutex_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
size_t SegregatedMultiStorage<PageSize>::Hash::operator()(const Key& key) const
{
  auto h1 = std::hash<size_t>{}(key.first);
  auto h2 = std::hash<size_t>{}(key.second);   
  h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
  return h1;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T, typename... Args>
T* SegregatedMultiStorage<PageSize>::allocate(Args&&... args)
{
  auto storage = findStorage<T>();
  if (!storage) {
    storage = createStorage<T>();
  }
  std::byte* bytePtr = storage->allocate();
  try {
    return ::new(bytePtr) T(std::forward<Args>(args)...);
  }
  catch (...) {
    storage->free(bytePtr);
    throw;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T>
IRawSegregatedStorage* SegregatedMultiStorage<PageSize>::findStorage()
{
  std::shared_lock lock(mutex_);
  auto key = std::make_pair(sizeof(T), alignof(T));
  auto it = storage_.find(key);
  return it == storage_.end()
    ? nullptr
    : it->second.get();
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T>
IRawSegregatedStorage* SegregatedMultiStorage<PageSize>::createStorage()
{
  std::unique_lock lock(mutex_);
  using Storage = RawSegregatedStorage<sizeof(T), alignof(T), Config<T>::pageSize>;
  auto it = storage_.emplace(
    std::make_pair(sizeof(T), alignof(T)),
    std::make_unique<Storage>()
  );
  return it.first->second.get();
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T, typename... Args>
typename SegregatedMultiStorage<PageSize>::template SPtr<T> SegregatedMultiStorage<PageSize>::allocateShared(Args&&... args)
{
  return SPtr<T>(
    allocate<T, Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T, typename... Args>
typename SegregatedMultiStorage<PageSize>::template UPtr<T> SegregatedMultiStorage<PageSize>::allocateUnique(Args&&... args)
{
  return UPtr<T>(
    allocate<T, Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t PageSize>
template <typename T>
void SegregatedMultiStorage<PageSize>::free(T* ptr)
{
  static_assert(std::is_nothrow_destructible_v<T>, "T must have a nothrow destructor");
  std::destroy_at(ptr);
  auto key = std::make_pair(sizeof(T), alignof(T));
  auto storageIter = storage_.find(key);  
  auto bytePtr = reinterpret_cast<std::byte*>(ptr);
  storageIter->second->free(bytePtr);
}

} // !namespace sgs
} // !namespace yaga

#endif // !YAGA_SEGREGATED_STORAGE_SEGREGATED_MULTI_STORAGE
