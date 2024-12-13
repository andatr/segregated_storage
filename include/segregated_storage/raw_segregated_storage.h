#ifndef YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE
#define YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
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
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
struct RawSegregatedStoragePage
{
public:
  using Item = RawSegregatedStorageItem<Size, Alignment>;
  using PagePtr = RawSegregatedStoragePage<Size, Alignment, PageSize>*;
  static constexpr size_t itemSize = sizeof(Item);
  static constexpr size_t itemCount = PageSize / itemSize;
  static_assert(itemCount > 0, "Page size must be large enough to fit at least one item");

public:
  explicit RawSegregatedStoragePage(PagePtr nextPage);
  Item* head() { return &items_[0]; }
  Item* tail() { return &items_[itemCount - 1]; }
  PagePtr nextPage() { return nextPage_; } 

private:
  PagePtr nextPage_;
  Item items_[itemCount];
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
RawSegregatedStoragePage<Size, Alignment, PageSize>::RawSegregatedStoragePage(PagePtr nextPage) :
  nextPage_(nextPage)
{
  items_[itemCount - 1].next = nullptr;
  for (size_t i = 1; i < itemCount; ++i) {
    items_[i - 1].next = &items_[i];
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
class IRawSegregatedStorage
{
public:
  virtual ~IRawSegregatedStorage() {}
  virtual std::byte* allocate() = 0;
  virtual void free(std::byte* ptr) = 0;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
class RawSegregatedStorage : public IRawSegregatedStorage
{
public:
  RawSegregatedStorage();
  ~RawSegregatedStorage();
  std::byte* allocate() override;
  void free(std::byte* ptr) override;

private:
  using Item = RawSegregatedStorageItem<Size, Alignment>;
  using Page = RawSegregatedStoragePage<Size, Alignment, PageSize>;

private:
  void addPage(uint64_t oldPageCount);
  void push(Item* head, Item* tail);

private:
  std::atomic_uint64_t pageCount_;
  std::atomic<Item*> freeItems_;
  Page* pagesHead_;
  std::mutex allocationMutex_;
};

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
RawSegregatedStorage<Size, Alignment, PageSize>::RawSegregatedStorage() :
  pageCount_(0),
  freeItems_(nullptr),
  pagesHead_(nullptr)
{
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
RawSegregatedStorage<Size, Alignment, PageSize>::~RawSegregatedStorage()
{
  while (pagesHead_) {
    auto next = pagesHead_->nextPage();
    delete pagesHead_;
    pagesHead_ = next;
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
std::byte* RawSegregatedStorage<Size, Alignment, PageSize>::allocate()
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
  } while (oldHead == nullptr || !freeItems_.compare_exchange_weak(oldHead, nextHead));
  return oldHead->body;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
void RawSegregatedStorage<Size, Alignment, PageSize>::addPage(uint64_t oldPageCount)
{
  std::lock_guard<std::mutex> lock(allocationMutex_);
  if (oldPageCount != pageCount_) return;
  pagesHead_ = new Page(pagesHead_);
  push(pagesHead_->head(), pagesHead_->tail());
  ++pageCount_;
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
void RawSegregatedStorage<Size, Alignment, PageSize>::free(std::byte* ptr)
{
  ptr -= offsetof(Item, body);
  Item* item = reinterpret_cast<Item*>(ptr);
  push(item, item);
}

// -----------------------------------------------------------------------------------------------------------------------------
template <size_t Size, size_t Alignment, size_t PageSize>
void RawSegregatedStorage<Size, Alignment, PageSize>::push(Item* head, Item* tail)
{
  auto oldHead = freeItems_.load();
  do {
    tail->next = oldHead;
  }
  while (!freeItems_.compare_exchange_weak(oldHead, head));
}

} // !namespace sgs
} // !namespace yaga

#endif // !YAGA_SEGREGATED_STORAGE_RAW_SEGREGATED_STORAGE
