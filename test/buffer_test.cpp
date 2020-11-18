#include "doctest/doctest.h"
#include "io/buffer.hpp"
#include <string>

using namespace std;
using namespace mrpc;

TEST_SUITE_BEGIN("buffer test");

string rand_string(size_t s)
{
  string str;
  for (int i = 0; i < s; i++)
  {
    str += (char)(rand() % 26 + 'A');
  }
  return str;
}

void rappend(buffer_t &buf, size_t s)
{
  buf.append(rand_string(s));
}

TEST_CASE("buffer test1")
{
  buffer_t buf;
  rappend(buf, default_net_buffer_size - 1);
  buf.append("+hello world");
  CHECK(buf.content_size() == default_net_buffer_size + 11);
  buf.reader_goahead(default_net_buffer_size - 1);
  CHECK(buf.remain_string() == "+hello world");
}

TEST_CASE("buffer test2")
{
#define bufs_size 2
  struct BUF
  {
    size_t buf_len;
    byte *buf = nullptr;
  };
  BUF bufs[bufs_size];

  auto get_buf = [](BUF *bufs, buffer_t &buffer) {
    return buffer.get_remain_buf_for_append(bufs, bufs_size, [](BUF *nbuf, byte *ibuf, size_t len) {
      nbuf->buf = ibuf;
      nbuf->buf_len = len;
    });
  };

  buffer_t buf;
  rappend(buf, default_net_buffer_size + 1);
  buf.clear();
  size_t bl = get_buf(bufs, buf);
  REQUIRE(bl == bufs_size);
  CHECK(bufs[0].buf_len == default_net_buffer_size);
  CHECK(bufs[1].buf_len > 0);
  memcpy(bufs[0].buf, "hello world", 11);
  buf.writer_goahead(11);
  CHECK(buf.to_string() == "hello world");

  string rnds = rand_string(default_net_buffer_size + 1 - 11);
  buf.append(rnds);
  CHECK(buf.to_string() == "hello world" + rnds);
}

TEST_CASE("buffer test3")
{
#define bufs_size 2
  struct BUF
  {
    size_t buf_len;
    byte *buf = nullptr;
  };
  BUF bufs[bufs_size];

  auto get_buf = [](BUF *bufs, buffer_t &buffer) {
    return buffer.append_remain_msg_to_array(bufs, bufs_size, [](BUF *nbuf, byte *ibuf, size_t len) {
      nbuf->buf = ibuf;
      nbuf->buf_len = len;
    });
  };

  buffer_t buf;
  string rnds = rand_string(default_net_buffer_size);
  buf.append(rnds);
  buf.append("hello world");
  size_t bl = get_buf(bufs, buf);
  REQUIRE(bl == bufs_size);
  CHECK(string((char *)bufs[0].buf, bufs[0].buf_len) +
            string((char *)bufs[1].buf, bufs[1].buf_len) ==
        rnds + "hello world");
}

TEST_SUITE_END();