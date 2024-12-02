#ifndef YAGA_SEGREGATED_STORAGE_TEST_TEST_CLASS
#define YAGA_SEGREGATED_STORAGE_TEST_TEST_CLASS

#include <atomic>

// -----------------------------------------------------------------------------------------------------------------------------
class TestClass
{
public:
  TestClass() : character_('B'), number_(123) {}
  ~TestClass() { ++dtorCallCount_; }
  char& character() { return character_; }
  int& number() { return number_; }
  static size_t dtorCallCount() { return dtorCallCount_; }

private:
  static std::atomic<size_t> dtorCallCount_;
  char character_;
  int number_;
};

#endif // !YAGA_SEGREGATED_STORAGE_TEST_TEST_CLASS
