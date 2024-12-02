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

void* operator new(std::size_t size);
void operator delete(void* ptr) noexcept;
void operator delete(void* ptr, std::size_t) noexcept;

#endif // !YAGA_SEGREGATED_STORAGE_TEST_MEMORY_HELPER
