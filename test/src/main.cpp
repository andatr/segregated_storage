#include "memory_helper.h"
#include "test_class.h"
#include <cstdlib>

std::atomic<size_t> MemoryHelper::allocationCount_ = 0;
std::atomic<size_t> TestClass::dtorCallCount_ = 0;

#if defined(_MSC_VER)

// -----------------------------------------------------------------------------------------------------------------------------
inline void* aligned_alloc(std::size_t alignment, std::size_t size)
{
  return _aligned_malloc(size, alignment);
}

// -----------------------------------------------------------------------------------------------------------------------------
inline void aligned_free(void* ptr)
{
  _aligned_free(ptr);
}

#else // _MSC_VER

// -----------------------------------------------------------------------------------------------------------------------------
inline void* aligned_alloc(std::size_t alignment, std::size_t size)
{
  return std::aligned_alloc(alignment, size);
}

// -----------------------------------------------------------------------------------------------------------------------------
inline void aligned_free(void* ptr) {
  std::free(ptr);
}

#endif // !_MSC_VER

// -----------------------------------------------------------------------------------------------------------------------------
void* operator new(std::size_t size)
{
  ++MemoryHelper::allocationCount_ ;
  void* ptr = std::malloc(size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}

// -----------------------------------------------------------------------------------------------------------------------------
void* operator new(std::size_t size, std::align_val_t alignment)
{
  ++MemoryHelper::allocationCount_ ;
  void* ptr = aligned_alloc(static_cast<std::size_t>(alignment), size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}

// -----------------------------------------------------------------------------------------------------------------------------
void operator delete(void* ptr) noexcept
{
  std::free(ptr);
}

// -----------------------------------------------------------------------------------------------------------------------------
void operator delete(void* ptr, std::size_t) noexcept
{
  std::free(ptr);
}

// -----------------------------------------------------------------------------------------------------------------------------
void operator delete(void* ptr, std::align_val_t) noexcept
{
  aligned_free(ptr);
}

#define BOOST_TEST_MODULE YAGA_SEGREGATED_STORAGE
#include <boost/test/unit_test.hpp>