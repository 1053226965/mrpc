#pragma once

#include <exception>

class sys_exception_t: public std::exception
{
public:
  sys_exception_t(const char * str):
    _msg(str)
  {}

  char const* get_msg() { return _msg; }

private:
  const char *_msg;
};