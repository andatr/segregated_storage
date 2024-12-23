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
  using UPtr = std::unique_ptr<T, Deleter>;
  template <typename T>
  using SPtr = std::shared_ptr<T>;
  template <typename T>
  struct TypePageSize
  {
    using Type = T;
    size_t pageSize;
    TypePageSize(size_t size) : pageSize(size) {}
  };

public:
  template <typename... TypeMappings>
  explicit SegregatedMultiStorage(size_t pageSize = DEFAULT_PAGE_SIZE, TypeMappings... mappings);
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
  void registerType(const TypePageSize<T>& mapping);
  template <typename T>
  IRawSegregatedStorage* findStorage();
  template <typename T>
  IRawSegregatedStorage* createStorage(size_t pageSize);

private:
  size_t pageSize_;
  std::unordered_map<Key, StoragePtr, Hash> storage_;
  std::shared_mutex mutex_;
};

// -----------------------------------------------------------------------------------------------------------------------------
inline size_t SegregatedMultiStorage::Hash::operator()(const Key& key) const
{
  auto h1 = std::hash<size_t>{}(key.first);
  auto h2 = std::hash<size_t>{}(key.second);   
  h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
  return h1;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename... TypeMappings>
SegregatedMultiStorage::SegregatedMultiStorage(size_t pageSize, TypeMappings... mappings) :
  pageSize_(pageSize)
{
  (registerType(mappings), ...);
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
void SegregatedMultiStorage::registerType(const TypePageSize<T>& mapping)
{
  if (!findStorage<T>()) {
    createStorage<T>(mapping.pageSize);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, typename... Args>
T* SegregatedMultiStorage::allocate(Args&&... args)
{
  auto storage = findStorage<T>();
  if (!storage) {
    storage = createStorage<T>(pageSize_);
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
template <typename T>
IRawSegregatedStorage* SegregatedMultiStorage::findStorage()
{
  std::shared_lock lock(mutex_);
  auto key = std::make_pair(sizeof(T), alignof(T));
  auto it = storage_.find(key);
  return it == storage_.end()
    ? nullptr
    : it->second.get();
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
IRawSegregatedStorage* SegregatedMultiStorage::createStorage(size_t pageSize)
{
  std::unique_lock lock(mutex_);
  using Storage = RawSegregatedStorage<sizeof(T), alignof(T)>;
  auto it = storage_.emplace(
    std::make_pair(sizeof(T), alignof(T)),
    std::make_unique<Storage>(pageSize)
  );
  return it.first->second.get();
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, typename... Args>
typename SegregatedMultiStorage::template SPtr<T> SegregatedMultiStorage::allocateShared(Args&&... args)
{
  return SPtr<T>(
    allocate<T, Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, typename... Args>
typename SegregatedMultiStorage::template UPtr<T> SegregatedMultiStorage::allocateUnique(Args&&... args)
{
  return UPtr<T>(
    allocate<T, Args...>(std::forward<Args>(args)...),
    Deleter { this }
  );
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
void SegregatedMultiStorage::free(T* ptr)
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
