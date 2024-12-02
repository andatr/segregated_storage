#include "memory_helper.h"
#include "test_class.h"

std::atomic<size_t> MemoryHelper::allocationCount_ = 0;
std::atomic<size_t> TestClass::dtorCallCount_ = 0;

// -----------------------------------------------------------------------------------------------------------------------------
void* operator new(std::size_t size)
{
  ++MemoryHelper::allocationCount_ ;
  void* ptr = std::malloc(size);
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

#define BOOST_TEST_MODULE YAGA_SEGREGATED_STORAGE
#include <boost/test/unit_test.hpp>