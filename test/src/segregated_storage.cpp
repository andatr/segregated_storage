#include <thread>
#include "segregated_storage/segregated_storage.h"
#include "memory_helper.h"
#include "test_class.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(SegregatedStorageTest)

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Ctor)
{
  yaga::sgs::SegregatedStorage<TestClass> pool;
  auto obj = pool.allocate();
  BOOST_TEST(obj != nullptr); 
  BOOST_TEST(obj->character() == 'B');
  BOOST_TEST(obj->number() == 123);
  pool.free(obj);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Dtor)
{
  int dtorCount = TestClass::dtorCallCount();
  yaga::sgs::SegregatedStorage<TestClass> pool;
  auto obj = pool.allocate();  
  pool.free(obj);
  BOOST_TEST(dtorCount + 1 == TestClass::dtorCallCount());
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Alignment)
{
  constexpr size_t ALIGNMENT = 256;
  struct alignas(ALIGNMENT) AlignedClass
  {
    char ch;
  };

  yaga::sgs::SegregatedStorage<AlignedClass> pool;
  auto obj1 = pool.allocate();
  auto obj2 = pool.allocate();
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
  yaga::sgs::SegregatedStorage<TestClass> pool;
  {
    auto obj = pool.allocateShared();
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
  yaga::sgs::SegregatedStorage<TestClass> pool;
  {
    auto obj = pool.allocateUnique();
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

  yaga::sgs::SegregatedStorage<Dependant> pool;
  {
    auto obj = pool.allocate(NonCopyable(123));
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->dependency().value() == 123);
    pool.free(obj);
  }
  {
    auto sobj = pool.allocateShared(NonCopyable(456));
    BOOST_TEST(sobj != nullptr); 
    BOOST_TEST(sobj->dependency().value() == 456);
  }
  {
    auto uobj = pool.allocateUnique(NonCopyable(789));
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
  yaga::sgs::SegregatedStorage<TestClass> pool(pageSize);

  TestClass* objects[objectCount + 1] {};
  size_t allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate();
  }
  BOOST_TEST(MemoryHelper::allocationCount() - allocationCount == pageCount);
  for (size_t i = 0; i < objectCount; ++i) {
    pool.free(objects[i]);
  }

  allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate();
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
  objects[objectCount] = pool.allocate();
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
  constexpr size_t itemSize = sizeof(typename RawStorate::Item);
  constexpr size_t objectsPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectsPerPage - 1) + sizeof(typename RawStorate::Page);
  yaga::sgs::SegregatedStorage<ThrowableCtor> pool(pageSize);

  try
  {
    pool.allocate(true);
    BOOST_TEST(false);
  }
  catch (CustomError& exception)
  {
    BOOST_TEST(true);
  }

  ThrowableCtor* objects[objectsPerPage + 1] {};
  size_t allocationCount = MemoryHelper::allocationCount();
  for (size_t i = 0; i < objectsPerPage; ++i) {
    objects[i] = pool.allocate(false);
  }
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount);
  objects[objectsPerPage] = pool.allocate(false);
  BOOST_TEST(MemoryHelper::allocationCount() == allocationCount + 1);
  for (size_t i = 0; i < objectsPerPage + 1; ++i) {
    pool.free(objects[i]);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ParallelAllocation)
{
  using RawStorate = yaga::sgs::RawSegregatedStoragePage<sizeof(TestClass), alignof(TestClass)>;
  constexpr size_t itemSize = sizeof(typename RawStorate::Item);
  constexpr int objectCount = 10000;
  constexpr size_t objectsPerPage = 3;
  constexpr size_t pageSize = itemSize * (objectsPerPage - 1) + sizeof(typename RawStorate::Page);

  yaga::sgs::SegregatedStorage<TestClass> pool(pageSize);

  std::vector<TestClass*> objects1(objectCount);
  for (int i = 0; i < objectCount; ++i) {
    objects1[i] = pool.allocate();
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
      objects2[i] = pool.allocate();
      objects2[i]->number() = objectCount + i;
    }
  });

  std::vector<TestClass*> objects3(objectCount);
  std::thread thread3([&pool, &objects3]() {
    for (int i = 0; i < objectCount; ++i) {
      objects3[i] = pool.allocate();
      objects3[i]->number() = 2 * objectCount + i;
    }
  });

  thread1.join();
  thread2.join();
  thread3.join();

  /*for (int i = 0; i < objectCount; ++i) {
    BOOST_TEST(objects2[i]->number() == objectCount + i);
    BOOST_TEST(objects3[i]->number() == 2 * objectCount + i);
  }*/

  for (int i = 0; i < objectCount; ++i) {
    pool.free(objects2[i]);
    pool.free(objects3[i]);
  }
}

// should throw an assert during compliation
// -----------------------------------------------------------------------------------------------------------------------------
//BOOST_AUTO_TEST_CASE(ThrowableDtor)
//{
//  class ThrowableClass
//  {
//    ~ThrowableClass() noexcept(false) {}
//  };
//
//  yaga::ObjectPool<ThrowableClass> pool;
//}

// should throw an assert during compliation
// -----------------------------------------------------------------------------------------------------------------------------
//BOOST_AUTO_TEST_CASE(PoolSizeAssert)
//{
//  yaga::ObjectPool<BasicClass, sizeof(BasicClass) - 1> pool;
//}

BOOST_AUTO_TEST_SUITE_END() // !SegregatedStorageTest
