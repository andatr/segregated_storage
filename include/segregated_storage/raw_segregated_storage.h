#ifndef YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE
#define YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace yaga {
namespace sgs {

constexpr size_t DEFAULT_PAGE_SIZE = 0x1000; 

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
struct RawSegregatedStorageItem
{
  RawSegregatedStorageItem* next;
  alignas(Alignment) std::byte body[Size];
  
  RawSegregatedStorageItem() : next(nullptr) {}
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
struct RawSegregatedStoragePage
{
public:
  using Item = RawSegregatedStorageItem<Size, Alignment>;
  using Page = RawSegregatedStoragePage<Size, Alignment>;

public:
  Page* nextPage() { return nextPage_; } 
  static Page* allocate(size_t pageSize, Page* nextPage, Item** head, Item** tail);
  static void free(Page* page);

private:
  explicit RawSegregatedStoragePage(Page* nextPage);
  void init(size_t itemCount, Item** head, Item** tail);

private:
  Page* nextPage_;
  Item items_;
};

// -----------------------------------------------------------------------------------------------------------------------------
class IRawSegregatedStorage
{
public:
  virtual ~IRawSegregatedStorage() {}
  virtual std::byte* allocate() = 0;
  virtual void free(std::byte* ptr) = 0;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
class RawSegregatedStorage : public IRawSegregatedStorage
{
public:
  RawSegregatedStorage(size_t pageSize = DEFAULT_PAGE_SIZE);
  ~RawSegregatedStorage();
  std::byte* allocate() override;
  void free(std::byte* ptr) override;

private:
  using Item = RawSegregatedStorageItem<Size, Alignment>;
  using Page = RawSegregatedStoragePage<Size, Alignment>;

private:
  void addPage(uint64_t oldPageCount);
  void push(Item* head, Item* tail);

private:
  size_t pageSize_;
  std::atomic_uint64_t pageCount_;
  std::atomic<Item*> freeItems_;
  Page* pageHead_;
  std::mutex allocationMutex_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
typename RawSegregatedStoragePage<Size, Alignment>::Page* 
RawSegregatedStoragePage<Size, Alignment>::allocate(size_t pageSize, Page* nextPage, Item** head, Item** tail)
{
  if (pageSize < sizeof(Page)) {
    throw std::runtime_error("Page size must be large enough to fit at least one item");
  }
  size_t itemsSize = pageSize - sizeof(Page);
  size_t itemCount = itemsSize / sizeof(Item) + 1;
  auto memory = operator new(pageSize, std::align_val_t(alignof(Page)));
  auto page = new (memory) Page(nextPage);
  page->init(itemCount, head, tail);
  return page;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
void RawSegregatedStoragePage<Size, Alignment>::free(Page* page)
{
  size_t s = alignof(Page);
  operator delete(page, std::align_val_t(s));
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
RawSegregatedStoragePage<Size, Alignment>::RawSegregatedStoragePage(Page* nextPage) :
  nextPage_(nextPage)
{
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
void RawSegregatedStoragePage<Size, Alignment>::init(size_t itemCount, Item** head, Item** tail)
{
  auto items = &items_;
  items[itemCount - 1].next = nullptr;
  for (size_t i = 1; i < itemCount; ++i) {
    items[i - 1].next = &items[i];
  }
  *head = &items[0];
  *tail = &items[itemCount - 1];
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
RawSegregatedStorage<Size, Alignment>::RawSegregatedStorage(size_t pageSize) :
  pageSize_(pageSize),
  pageCount_(0),
  freeItems_(nullptr),
  pageHead_(nullptr)
{
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
RawSegregatedStorage<Size, Alignment>::~RawSegregatedStorage()
{
  while (pageHead_) {
    auto next = pageHead_->nextPage();
    Page::free(pageHead_);
    pageHead_ = next;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
std::byte* RawSegregatedStorage<Size, Alignment>::allocate()
{
  auto pageCount = pageCount_.load();
  auto oldHead = freeItems_.load();
  Item* nextHead = nullptr;
  do {
    if (oldHead == nullptr) {
      addPage(pageCount);
      pageCount = pageCount_.load();
      oldHead = freeItems_.load();
    }
    if (oldHead != nullptr) {
      nextHead = oldHead->next;
    }
  } while (oldHead == nullptr || !freeItems_.compare_exchange_weak(oldHead, nextHead, std::memory_order_release, std::memory_order_relaxed));
  return oldHead->body;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
void RawSegregatedStorage<Size, Alignment>::addPage(uint64_t oldPageCount)
{
  std::lock_guard<std::mutex> lock(allocationMutex_);
  if (oldPageCount != pageCount_) return;
  Item* head = nullptr;
  Item* tail = nullptr;
  pageHead_ = Page::allocate(pageSize_, pageHead_, &head, &tail);
  push(head, tail);
  pageSize_ *= 2;
  ++pageCount_;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
void RawSegregatedStorage<Size, Alignment>::free(std::byte* ptr)
{
  ptr -= offsetof(Item, body);
  Item* item = reinterpret_cast<Item*>(ptr);
  push(item, item);
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment>
void RawSegregatedStorage<Size, Alignment>::push(Item* head, Item* tail)
{
  auto oldHead = freeItems_.load();
  do {
    tail->next = oldHead;
  }
  while (!freeItems_.compare_exchange_weak(oldHead, head, std::memory_order_release, std::memory_order_relaxed));
}

} // !namespace sgs
} // !namespace yaga

#endif // !YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE
