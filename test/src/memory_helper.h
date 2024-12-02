#ifndef YAGA_SEGREGATED_STORAGE_TEST_MEMORY_HELPER
#define YAGA_SEGREGATED_STORAGE_TEST_MEMORY_HELPER

#include <atomic>
#include <new>

// -----------------------------------------------------------------------------------------------------------------------------
class MemoryHelper
{
friend void* operator new(std::size_t size);

public:
  static size_t allocationCount() { return allocationCount_; }

private:
  static std::atomic<size_t> allocationCount_;
};

// -----------------------------------------------------------------------------------------------------------------------------
inline void* operator new(std::size_t size)
{
  ++MemoryHelper::allocationCount_ ;
  void* ptr = std::malloc(size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}

// -----------------------------------------------------------------------------------------------------------------------------
inline void operator delete(void* ptr) noexcept
{
  std::free(ptr);
}

// -----------------------------------------------------------------------------------------------------------------------------
inline void operator delete(void* ptr, std::size_t) noexcept
{
  std::free(ptr);
}

#endif // !YAGA_SEGREGATED_STORAGE_TEST_MEMORY_HELPER
