#include "object_pool.h"
#include <iostream>
#include <boost/test/unit_test.hpp>

static std::atomic<size_t> gAllocationCount = 0;

// -----------------------------------------------------------------------------------------------------------------------------
void* operator new(std::size_t size)
{
  ++gAllocationCount;
  void* ptr = std::malloc(size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}

// -----------------------------------------------------------------------------------------------------------------------------
void operator delete(void* ptr) noexcept
{
  std::free(ptr);
}

BOOST_AUTO_TEST_SUITE(ObjectPoolTest)

// -----------------------------------------------------------------------------------------------------------------------------
class BasicClass
{
public:

  BasicClass() :
    ch('B'),
    num(123)
  {}

  ~BasicClass()
  {
    ++dtorCount;
  }

  static int dtorCount;

  char ch;
  int num;
};

int BasicClass::dtorCount = 0;

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Ctor)
{
  yaga::ObjectPool<BasicClass> pool;
  auto obj = pool.allocate();
  BOOST_TEST(obj != nullptr); 
  BOOST_TEST(obj->ch == 'B');
  BOOST_TEST(obj->num == 123);
  pool.free(obj);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Dtor)
{
  int dtorCount = BasicClass::dtorCount;
  yaga::ObjectPool<BasicClass> pool;
  auto obj = pool.allocate();  
  pool.free(obj);
  BOOST_TEST(dtorCount + 1 == BasicClass::dtorCount);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(Alignment)
{
  constexpr size_t ALIGNMENT = 16;

  struct alignas(ALIGNMENT) AlignedClass
  {
    char ch;
  };

  yaga::ObjectPool<AlignedClass> pool;
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
  int dtorCount = BasicClass::dtorCount;
  yaga::ObjectPool<BasicClass> pool;
  {
    auto obj = pool.allocateShared();
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->ch == 'B');
    BOOST_TEST(obj->num == 123);
  }
  BOOST_TEST(dtorCount + 1 == BasicClass::dtorCount);
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(AllocateUnique)
{
  int dtorCount = BasicClass::dtorCount;
  yaga::ObjectPool<BasicClass> pool;
  {
    auto obj = pool.allocateUnique();
    BOOST_TEST(obj != nullptr); 
    BOOST_TEST(obj->ch == 'B');
    BOOST_TEST(obj->num == 123);
  }
  BOOST_TEST(dtorCount + 1 == BasicClass::dtorCount);
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

  yaga::ObjectPool<Dependant> pool;
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
    auto uobj = pool.allocateShared(NonCopyable(789));
    BOOST_TEST(uobj != nullptr); 
    BOOST_TEST(uobj->dependency().value() == 789);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(PageAllocation)
{
  constexpr size_t itemSize = sizeof(yaga::opool::ObjectPoolItem<BasicClass>);
  constexpr size_t objectCount = 100;
  constexpr size_t multiplier = 3;
  constexpr size_t pageSize = itemSize * multiplier;
  constexpr size_t pageCount = (objectCount + multiplier - 1) / multiplier;
  yaga::ObjectPool<BasicClass, pageSize> pool;

  BasicClass* objects[objectCount];
  
  size_t allocationCount = gAllocationCount;
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate();
  }
  // one allocation per page, and some allocations to resize page storage (vector)
  BOOST_TEST(gAllocationCount - allocationCount <= 2 * pageCount);
  
  for (size_t i = 0; i < objectCount; ++i) {
    pool.free(objects[i]);
  }

  allocationCount = gAllocationCount;
  for (size_t i = 0; i < objectCount; ++i) {
    objects[i] = pool.allocate();
  }
  BOOST_TEST(gAllocationCount - allocationCount == 0);

  for (size_t i = 0; i < objectCount; ++i) {
    pool.free(objects[i]);
  }
}

// -----------------------------------------------------------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(ParallelAllocation)
{
  constexpr size_t itemSize = sizeof(yaga::opool::ObjectPoolItem<BasicClass>);
  constexpr size_t objectCount = 1000;
  constexpr size_t multiplier = 3;
  constexpr size_t pageSize = itemSize * multiplier;

  yaga::ObjectPool<BasicClass, pageSize> pool;

  std::vector<BasicClass*> objects1(objectCount);
  for (size_t i = 0; i < objectCount; ++i) {
    objects1[i] = pool.allocate();
    objects1[i]->num = i;
  }
  
  std::thread thread1([&pool, &objects1]() {
    for (size_t i = 0; i < objectCount; ++i) {
      pool.free(objects1[i]);
    }
  });

  std::vector<BasicClass*> objects2(objectCount);
  std::thread thread2([&pool, &objects2]() {
    for (size_t i = 0; i < objectCount; ++i) {
      objects2[i] = pool.allocate();
      objects2[i]->num = 1000 + i;
    }
  });

  std::vector<BasicClass*> objects3(objectCount);
  std::thread thread3([&pool, &objects3]() {
    for (size_t i = 0; i < objectCount; ++i) {
      objects3[i] = pool.allocate();
      objects3[i]->num = 2000 + i;
    }
  });

  thread1.join();
  thread2.join();
  thread3.join();

  for (size_t i = 0; i < objectCount; ++i) {
    BOOST_TEST(objects2[i]->num == 1000 + i);
    BOOST_TEST(objects3[i]->num == 2000 + i);
  }

  for (size_t i = 0; i < objectCount; ++i) {
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

BOOST_AUTO_TEST_SUITE_END() // !ObjectPoolTest