#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "common/debug/log.hpp"
#include "common/coroutine/task.hpp"
#include "common/coroutine/sync_wait.hpp"
#include <cstdlib>
#include <string>

using namespace std;
using namespace mrpc;

struct A
{
  ~A()
  {
    printf("~A\n");
  }
};

task_t<void> func(A &a)
{
  co_await std::experimental::suspend_always();
}

int main(int argc, char **argv)
{
  {
    A a;
    auto b = func(a);
  }
  printf("A\n");
  init_g_logger();

  doctest::Context context;

  // !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

  // defaults
  context.addFilter("test-case-exclude", "*math*"); // exclude test cases with "math" in their name
  context.setOption("abort-after", 5);              // stop test execution after 5 failed assertions
  context.setOption("order-by", "name");            // sort the test cases by their name

  context.applyCommandLine(argc, argv);

  // overrides
  context.setOption("no-breaks", true); // don't break in the debugger when assertions fail

  int res = context.run(); // run

  if (context.shouldExit()) // important - query flags (and --exit) rely on the user doing this
    return res;             // propagate the result of the tests
}