#include <thread>
#include "segregated_storage/segregated_multi_storage.h"
#include "memory_helper.h"
#include "test_class.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(SegregatedMultiStorageTest)

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Ctor)
{
  yaga::sgs::SegregatedMultiStorage pool;
  auto obj = pool.allocate<TestClass>();
  BOOST_TEST(obj != nullptr); 
  BOOST_TEST(obj->character() == 'B');
  BOOST_TEST(obj->number() == 123);
  pool.free(obj);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Dtor)
{
  int dtorCount = TestClass::dtorCallCount();
  yaga::sgs::SegregatedMultiStorage pool;
  auto obj = pool.allocate<TestClass>();  
  pool.free(obj);
  BOOST_TEST(dtorCount + 1 == TestClass::dtorCallCount());
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Alignment)
{
  constexpr size_t ALIGNMENT = 16;
  struct alignas(ALIGNMENT) AlignedClass
  {
    char ch;
  };
  yaga::sgs::SegregatedMultiStorage pool;
  auto obj1 = pool.allocate<AlignedClass>();
  auto obj2 = pool.allocate<AlignedClass>();
  uintptr_t address1 = reinterpret_cast<uintptr_t>(obj1);
  uintptr_t address2 = reinterpret_cast<uintptr_t>(obj2);
  BOOST_TEST(address1 % ALIGNMENT == 0);
  BOOST_TEST(address2 % ALIGNMENT == 0);
  pool.free(obj2);
  pool.free(obj1);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AllocateShared)
{
  int dtorCount = TestClass::dtorCallCount();
  yaga::sgs::SegregatedMultiStorage pool;
  {
    auto obj = pool.allocateShared<TestClass>();
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->character() == 'B');
    BOOST_TEST(obj->number() == 123);
  }
  BOOST_TEST(dtorCount + 1 == TestClass::dtorCallCount());
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AllocateUnique)
{
  int dtorCount = TestClass::dtorCallCount();
  yaga::sgs::SegregatedMultiStorage pool;
  {
    auto obj = pool.allocateUnique<TestClass>();
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->character() == 'B');
    BOOST_TEST(obj->number() == 123);
  }
  BOOST_TEST(dtorCount + 1 == TestClass::dtorCallCount());
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PerfectForwarding)
{
  class NonCopyable
  {
  public:
    NonCopyable(int value) : value_(value) {}
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&& other) noexcept : value_(other.value_) { other.value_ = 0; }
    NonCopyable& operator=(NonCopyable&& other) noexcept {
      if (this == &other) return *this; 
      value_ = other.value_;
      other.value_ = 0;
      return *this; 
    }
    int& value() { return value_; }

  private:
    int value_;
  };

  class Dependant
  {
  public:
    explicit Dependant(NonCopyable&& dependency) : dependency_(std::move(dependency)) {}
    NonCopyable& dependency() { return dependency_; }

  private:
    NonCopyable dependency_;
  };

  yaga::sgs::SegregatedMultiStorage pool;
  {
    auto obj = pool.allocate<Dependant>(NonCopyable(123));
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->dependency().value() == 123);
    pool.free(obj);
  }
  {
    auto sobj = pool.allocateShared<Dependant>(NonCopyable(456));
    BOOST_TEST(sobj != nullptr); 
    BOOST_TEST(sobj->dependency().value() == 456);
  }
  {
    auto uobj = pool.allocateUnique<Dependant>(NonCopyable(789));
    BOOST_TEST(uobj != nullptr); 
    BOOST_TEST(uobj->dependency().value() == 789);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PageAllocation)
{
  using RawStorate = yaga::sgs::RawSegregatedStoragePage<sizeof(TestClass), alignof(TestClass)>;
  constexpr size_t itemSize = sizeof(typename RawStorate::Item);
  constexpr size_t objectsPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectsPerPage - 1) + sizeof(typename RawStorate::Page);
  constexpr size_t pageCount = 34;
  constexpr size_t objectCount = pageCount * objectsPerPage;
  yaga::sgs::SegregatedMultiStorage pool(
    yaga::sgs::DEFAULT_PAGE_SIZE,
    yaga::sgs::SegregatedMultiStorage::TypePageSize<TestClass>(pageSize)
  );

  TestClass* objects[objectCount + 1] {};

  size_t allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate<TestClass>();
  }
  BOOST_TEST(MemoryHelper::allocationCount() - allocationCount == pageCount);
  for (size_t i = 0; i < objectCount; ++i) {
    pool.free(objects[i]);
  }

  allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate<TestClass>();
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
  objects[objectCount] = pool.allocate<TestClass>();
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount + 1);
  for (size_t i = 0; i < objectCount + 1; ++i) {
    pool.free(objects[i]);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ExceptionInCtor)
{
  class CustomError : public std::runtime_error
  {
  public:
    explicit CustomError(const char* message) : std::runtime_error(message) {}
  };

  class ThrowableCtor
  {
  public:
    ThrowableCtor(bool throwException) : value_(0.0f) { if (throwException) { throw CustomError("Exception in ctor"); }}
    float& value() { return value_; }

  private:
    float value_;
  };

  using RawStorate = yaga::sgs::RawSegregatedStoragePage<sizeof(ThrowableCtor), alignof(ThrowableCtor)>;
  constexpr size_t itemSize = sizeof(RawStorate::Item);
  constexpr size_t objectPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectPerPage - 1) + sizeof(RawStorate::Page);
  yaga::sgs::SegregatedMultiStorage pool(pageSize);

  try
  {
    pool.allocate<ThrowableCtor>(true);
    BOOST_TEST(false);
  }
  catch (CustomError& exception)
  {
    BOOST_TEST(true);
  }

  ThrowableCtor* objects[objectPerPage + 1] {};
  size_t allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectPerPage; ++i) {
    objects[i] = pool.allocate<ThrowableCtor>(false);
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
  objects[objectPerPage] = pool.allocate<ThrowableCtor>(false);
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount + 1);
  for (size_t i = 0; i < objectPerPage + 1; ++i) {
    pool.free(objects[i]);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(DifferentBuckets)
{
  struct Bucket1
  {
    double value1;
    double value2;
  };

  struct Bucket2
  {
    int value;
  };

  yaga::sgs::SegregatedMultiStorage pool;

  constexpr size_t bucket1Size = 10;
  Bucket1* objects1[bucket1Size] {};
  for (size_t i = 0; i < bucket1Size; ++i) {
    objects1[i] = pool.allocate<Bucket1>();
  }
  for (size_t i = 0; i < bucket1Size; ++i) {
    pool.free(objects1[i]);
  }

  size_t allocationCount = MemoryHelper::allocationCount();
  constexpr size_t bucket2Size = 10;
  Bucket2* objects2[bucket2Size] {};
  for (size_t i = 0; i < bucket2Size; ++i) {
    objects2[i] = pool.allocate<Bucket2>();
  }
  BOOST_TEST(MemoryHelper::allocationCount() != allocationCount);
  for (size_t i = 0; i < bucket2Size; ++i) {
    pool.free(objects2[i]);
  }
 
  allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < bucket1Size; ++i) {
    objects1[i] = pool.allocate<Bucket1>();
  }
  for (size_t i = 0; i < bucket2Size; ++i) {
    objects2[i] = pool.allocate<Bucket2>();
  }
  for (size_t i = 0; i < bucket1Size; ++i) {
    pool.free(objects1[i]);
  }
  for (size_t i = 0; i < bucket2Size; ++i) {
    pool.free(objects2[i]);
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(SameBuckets)
{
  struct Bucket1
  {
    double value1;
    double value2;
  };

  struct alignas(Bucket1) Bucket2
  {
    char value[sizeof(Bucket1)];
  };

  using RawStorate = yaga::sgs::RawSegregatedStoragePage<sizeof(Bucket1), alignof(Bucket1)>;
  constexpr size_t itemSize = sizeof(RawStorate::Item);
  constexpr size_t objectPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectPerPage - 1) + sizeof(typename RawStorate::Page);
  yaga::sgs::SegregatedMultiStorage pool(pageSize);

  Bucket1* objects1[objectPerPage] {};
  for (size_t i = 0; i < objectPerPage; ++i) {
    objects1[i] = pool.allocate<Bucket1>();
  }
  for (size_t i = 0; i < objectPerPage; ++i) {
    pool.free(objects1[i]);
  }

  size_t allocationCount = MemoryHelper::allocationCount();
  Bucket1* objects2[objectPerPage] {};
  for (size_t i = 0; i < objectPerPage; ++i) {
    objects2[i] = pool.allocate<Bucket1>();
  }
  for (size_t i = 0; i < objectPerPage; ++i) {
    pool.free(objects2[i]);
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ParallelAllocation)
{
  struct TestClass2
  {
    long number;
    double value1;
    double value2;
  };

  using RawStorate = yaga::sgs::RawSegregatedStoragePage<sizeof(TestClass), alignof(TestClass)>;
  constexpr size_t itemSize = sizeof(typename RawStorate::Item);
  constexpr int objectCount = 10000;
  constexpr size_t objectsPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectsPerPage - 1) + sizeof(typename RawStorate::Page);

  yaga::sgs::SegregatedMultiStorage pool(pageSize);

  std::vector<TestClass*> objects1(objectCount);
  for (int i = 0; i < objectCount; ++i) {
    objects1[i] = pool.allocate<TestClass>();
    objects1[i]->number() = i;
  }
  
  std::thread thread1([&pool, &objects1]() {
    for (int i = 0; i < objectCount; ++i) {
      pool.free(objects1[i]);
    }
  });

  std::vector<TestClass*> objects2(objectCount);
  std::thread thread2([&pool, &objects2]() {
    for (int i = 0; i < objectCount; ++i) {
      objects2[i] = pool.allocate<TestClass>();
      objects2[i]->number() = objectCount + i;
    }
  });

  std::vector<TestClass*> objects3(objectCount);
  std::thread thread3([&pool, &objects3]() {
    for (int i = 0; i < objectCount; ++i) {
      objects3[i] = pool.allocate<TestClass>();
      objects3[i]->number() = 2 * objectCount + i;
    }
  });

  std::vector<TestClass2*> objects4(objectCount);
  std::thread thread4([&pool, &objects4]() {
    for (int i = 0; i < objectCount; ++i) {
      objects4[i] = pool.allocate<TestClass2>();
      objects4[i]->number = 3 * static_cast<long>(objectCount) + i;
    }
  });

  thread1.join();
  thread2.join();
  thread3.join();
  thread4.join();

  for (int i = 0; i < objectCount; ++i) {
    BOOST_TEST(objects2[i]->number() == objectCount + i);
    BOOST_TEST(objects3[i]->number() == 2 * objectCount + i);
    BOOST_TEST(objects4[i]->number   == 3 * static_cast<long>(objectCount) + i);
  }

  for (int i = 0; i < objectCount; ++i) {
    pool.free(objects2[i]);
    pool.free(objects3[i]);
    pool.free(objects4[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END() // !SegregatedMultiStorageTest
