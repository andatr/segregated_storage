#ifndef YAGA_OBJECT_POOL
#define YAGA_OBJECT_POOL

#include <atomic>
#include <list>
#include <memory>
#include <mutex>

namespace yaga {
namespace pool {

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T>
struct ObjectPoolItem
{
  ObjectPoolItem<T>* next;
  alignas(T) std::byte body[sizeof(T)];
};

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
struct ObjectPoolPage
{
public:
  using Item = ObjectPoolItem<T>;

public:
  ObjectPoolPage();
  Item* head() { return &items_[0]; }
  Item* tail() { return &items_[count - 1]; }

private:
  static constexpr size_t count = Size / sizeof(Item);
  Item items_[count];
};

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
ObjectPoolPage<T, Size>::ObjectPoolPage()
{
  items_[count - 1].next = nullptr;
  for (size_t i = 1; i < count; ++i) {
    items_[i - 1].next = &items_[i];
  }
}

template <typename T, int Size>
using ObjectPoolPageUPtr = std::unique_ptr<ObjectPoolPage<T, Size>>;

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
class ObjectPool
{
public:
  ObjectPool();
  ~ObjectPool();
  T* allocate();
  void free(T* ptr);

private:
  using ItemPtr = ObjectPoolItem<T>*;

private:
  void addPage(uint64_t oldPageCount);
  ItemPtr pop();
  void push(ItemPtr head, ItemPtr tail);

private:
  std::atomic_uint64_t pageCount_;
  std::atomic<ItemPtr> freeItems_;
  std::list<ObjectPoolPageUPtr<T, Size>> pages_;
  std::mutex allocationMutex_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
ObjectPool<T, Size>::ObjectPool() :
  pageCount_(0),
  freeItems_(nullptr)
{
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
ObjectPool<T, Size>::~ObjectPool()
{
  ItemPtr item = freeItems_;
  while (item) {
    T* objPtr = std::launder(reinterpret_cast<T*>(item->body));
    std::destroy_at(objPtr);
    item = item->next;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
T* ObjectPool<T, Size>::allocate()
{
  auto itemPtr = pop();
  T* objPtr = std::launder(reinterpret_cast<T*>(itemPtr->body));
  ::new(objPtr) T();
  return objPtr;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
ObjectPool<T, Size>::ItemPtr ObjectPool<T, Size>::pop()
{
  auto pageCount = pageCount_.load();
  auto oldHead = freeItems_.load();
  ItemPtr nextHead = nullptr;
  do {
    if (oldHead == nullptr) {
      addPage(pageCount);
      pageCount = pageCount_.load();
      oldHead = freeItems_.load();
      continue;
    }
    nextHead = oldHead->next;
  } while (!freeItems_.compare_exchange_weak(oldHead, nextHead));
  return oldHead;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
void ObjectPool<T, Size>::addPage(uint64_t oldPageCount)
{
  std::lock_guard<std::mutex> lock(allocationMutex_);
  if (oldPageCount != pageCount_) return;
  ++pageCount_;
  pages_.push_front(std::make_unique<ObjectPoolPage<T, Size>>());
  push(pages_.front()->head(), pages_.front()->tail());
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
void ObjectPool<T, Size>::free(T* ptr)
{
  std::destroy_at(ptr);
  auto bytePtr = reinterpret_cast<std::byte*>(ptr);
  bytePtr -= offsetof(ObjectPoolItem<T>, body);
  ItemPtr itemPtr = reinterpret_cast<ItemPtr>(bytePtr);
  push(itemPtr, itemPtr);
}

// -----------------------------------------------------------------------------------------------------------------------------
template <typename T, int Size>
void ObjectPool<T, Size>::push(ItemPtr head, ItemPtr tail)
{
  auto oldHead = freeItems_.load();
  do
  {
    tail->next = oldHead;
  }
  while (!freeItems_.compare_exchange_weak(oldHead, head));
}

} // !namespace opool

template <typename T, int PageSizeBytes = 64 * 1024>
using ObjectPool = pool::ObjectPool<T, PageSizeBytes>;

} // !namespace yaga

#endif // !YAGA_OBJECT_POOL
