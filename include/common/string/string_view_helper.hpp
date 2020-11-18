#pragma once
#include <string_view>
#include "common/debug/helper.hpp"

namespace mstring
{
  struct sring_view_trait
  {
    constexpr static size_t count(std::string_view src, std::string_view pattern)
    {
      size_t ret = 0;
      if (src.length() < pattern.length())
        return ret;

      for (size_t i = 0; i < src.size();)
      {
        if (std::string_view(&src[i], pattern.length()) == pattern)
        {
          ret++;
          i += pattern.length();
        }
        else
        {
          i++;
        }
      }
      return ret;
    }

    template<typename INT_T = std::int32_t>
    constexpr static INT_T to_integer(std::string_view src)
    {
      INT_T ret = 0;
      if (src.length() == 0)
        return ret;
      size_t i = 0;
      bool is_negetive = false;
      if (src[0] == '-')
      {
        is_negetive = true;
        i++;
      }
      for (; i < src.length(); i++)
      {
        if (src[i] < '0' || src[i] > '9')
          M_ASSERT(false);
        ret = ret * 10 + src[i] - '0';
      }
      if (is_negetive)
        ret = -ret;
      return ret;
    }
  };

  typedef sring_view_trait sv_trait;
}