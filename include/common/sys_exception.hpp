#pragma once

#include <exception>

class sys_exception_t: public std::exception
{
public:
  sys_exception_t(const char * str):
    msg_(str)
  {}

  char const* get_msg() { return msg_; }

private:
  const char *msg_;
};