#include <afina/lockfree/thread_local.h>
#include "gtest/gtest.h"
#include <iostream>
#include <set>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;
using namespace Afina::LockFree;

TEST(SimpleTest, OneThread) {
  ThreadLocal<int> storage(NULL);
  int* a = new int(3);
  storage.set(a);
  int* b = storage.get();
  EXPECT_TRUE(a == b);
  free(a);
}

TEST(SimpleTest, Destructor) {
  ThreadLocal<int> storage;
  int* a = new int(3);
  storage.set(a);
  EXPECT_TRUE(1);
}

std::mutex m; 
std::vector<int*> for_test;

void thread_body(void) {
  ThreadLocal<int> storage(NULL);
  std::srand(unsigned(std::time(0)));
  storage.set(new int(std::rand()));
  std::unique_lock<std::mutex> lk(m);
  for_test.push_back(storage.get());
}

TEST(SimpleTest, MultipleThreads) {
  thread t1(thread_body);
  thread t2(thread_body);
  t1.join();
  t2.join();
  EXPECT_TRUE(for_test[0] != for_test[1]);
}

