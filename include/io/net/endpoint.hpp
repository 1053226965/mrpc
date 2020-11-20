#pragma once
#include "common/platform.hpp"
#include "common/string/string_view_helper.hpp"

namespace mrpc
{
  namespace net
  {
#ifdef OS_WIN
    typedef SOCKADDR_IN sockaddr_in_t;
    typedef SOCKADDR_IN6 sockaddr_in6_t;
#elif defined(OS_GNU_LINUX)
    typedef sockaddr_in sockaddr_in_t;
    typedef sockaddr_in6 sockaddr_in6_t;
#endif
    class endpoint_t
    {
    public:
      enum class endpoint_type
      {
        IP_V4,
        IP_V6,
        INVALID
      };

      endpoint_t() noexcept;
      endpoint_t(std::string_view addr) noexcept;
      endpoint_t(sockaddr const &addr) noexcept;

      bool is_v4() const noexcept { return _type == endpoint_type::IP_V4; }
      bool is_v6() const noexcept { return _type == endpoint_type::IP_V6; }

      sockaddr const *get_sockaddr() const noexcept { return reinterpret_cast<sockaddr const *>(&_addr4); }
      sockaddr *get_sockaddr() noexcept { return reinterpret_cast<sockaddr*>(&_addr4); }
      size_t addr_len() const noexcept { return is_v4() ? sizeof(sockaddr_in_t) : is_v6() ? sizeof(sockaddr_in6_t) : 0; }

    private:
      union
      {
        sockaddr_in_t _addr4;
        sockaddr_in6_t _addr6;
      };
      endpoint_type _type;
    };

    inline endpoint_t::endpoint_t() noexcept
        : _addr6{0},
          _type(endpoint_type::INVALID)
    {
    }

    inline endpoint_t::endpoint_t(std::string_view addr) noexcept
        : _addr6{0},
          _type(endpoint_type::INVALID)
    {
      using namespace std;
      using namespace mstring;

      size_t pcount = sv_trait::count(addr, ".");
      if (sv_trait::count(addr, ":") == 1 && pcount == 3)
      {
        _type = endpoint_type::IP_V4;
        _addr4.sin_family = AF_INET;
        _addr4.sin_port = sv_trait::to_integer<decltype(_addr4.sin_port)>(addr.substr(addr.find(":") + 1));
        _addr4.sin_port = htons(_addr4.sin_port);

        uint32_t tmp = 0;
        union s_un
        {
          char ar[4];
          uint32_t iip;
        } ip;
        ip.iip = 0;
        for (size_t i = 0, j = 0; i < addr.find(":") + 1; i++)
        {
          if (addr[i] != '.' && addr[i] != ':')
          {
            tmp = tmp * 10 + addr[i] - '0';
          }
          else
          {
            ip.ar[j++] = tmp;
            tmp = 0;
          }
        }
#ifdef OS_WIN
        _addr4.sin_addr.S_un.S_addr = ip.iip;
#elif defined(OS_GNU_LINUX)
        _addr4.sin_addr.s_addr = ip.iip;
#endif
      }
    }

    inline endpoint_t::endpoint_t(sockaddr const &addr) noexcept
        : endpoint_t()
    {
      if (addr.sa_family == AF_INET)
      {
        _type = endpoint_type::IP_V4;
        memcpy(&_addr4, &addr, sizeof(_addr4));
      }
      else if (addr.sa_family == AF_INET)
      {
        _type = endpoint_type::IP_V6;
        memcpy(&_addr6, &addr, sizeof(_addr6));
      }
    }
  } // namespace net
} // namespace mrpc