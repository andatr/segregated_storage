#include "memory_helper.h"
#include "test_class.h"

std::atomic<size_t> MemoryHelper::allocationCount_ = 0;
std::atomic<size_t> TestClass::dtorCallCount_ = 0;

#define BOOST_TEST_MODULE YAGA_SEGREGATED_STORAGE
#include <boost/test/unit_test.hpp>