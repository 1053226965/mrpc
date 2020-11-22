#pragma once
#include <vector>
#include <functional>
#include <string_view>
#include <memory>
#include "common/ref_count.hpp"

namespace mrpc::detail
{
#ifndef byte
#define byte unsigned char
#endif
#define default_net_buffer_size 512
#define max_chunk_count 16

  template <typename T, typename ALLOCATOR = std::allocator<T>>
  class buffer_t
  {
  public:
    using allocator_type = ALLOCATOR;

    struct chunk_t
    {
      chunk_t(T *buf, size_t s, size_t c) noexcept;
      T *_buffer;
      size_t _size;
      size_t _capacity;
    };

    struct storage_t
    {
      storage_t(buffer_t &owner) noexcept;
      ~storage_t() noexcept;
      buffer_t &_owner;
      std::vector<chunk_t> _chunks;
      size_t _chunk_write_index;
      size_t _write_offset; // offset abourt _chunks[_chunk_write_index]
      size_t _chunk_read_index;
      size_t _read_offset;
    };

    buffer_t(allocator_type const &allocator = allocator_type());
    buffer_t(size_t size, allocator_type const &allocator = allocator_type());
    buffer_t(buffer_t const &buffer) noexcept;
    buffer_t(buffer_t &&buffer) noexcept;
    ~buffer_t() noexcept { _storage = nullptr; }

    buffer_t &operator=(buffer_t &&buffer) noexcept;
    buffer_t &operator=(buffer_t const &buffer) noexcept;

    void append(std::string_view const &s);

    /* 将剩余空间append到array里 
     * @param array: buf array
     * @param func: FUNC(BUF*, T*, size_t) */
    template <typename BUF_ARRAY, typename FUNC>
    size_t get_remain_buf_for_append(BUF_ARRAY &&array, size_t len, FUNC &&func);

    /* 将剩余未读内容append到array里
     * @param array: buf array
     * @param func: FUNC(BUF*, T*, size_t) */
    template <typename BUF_ARRAY, typename FUNC>
    size_t append_remain_msg_to_array(BUF_ARRAY &&array, size_t len, FUNC &&func);

    /* 写指针前移，跟get_remain_buf_for_append配合使用 */
    void writer_goahead(size_t bytes) noexcept;

    /* 读指针前移，跟append_remain_msg_to_array配合使用 */
    void reader_goahead(size_t bytes) noexcept;

    void for_each(std::function<void(chunk_t &ck)> const &func);

    bool is_eof() noexcept;
    bool is_no_bufs() noexcept;

    auto &back_chunk() { return _storage->_chunks[_storage->_chunk_write_index]; }
    size_t chunks_size() const noexcept { return _storage->_chunks.size(); }
    size_t remain_chunks_size() const noexcept { return _storage->_chunks.size() - _storage->_chunk_write_index; }
    size_t content_size() const noexcept;
    void clear() noexcept;

    void go_to_start() noexcept;
    void go_to_end() noexcept;

    std::string remain_string();
    std::string to_string();

  private:
    void own_buf(T *buf, size_t len, size_t capacity);
    T *allocate(size_t capacity);
    void deallocate(T *p, size_t capacity);

  private:
    allocator_type _allocator;
    std::shared_ptr<storage_t> _storage;
  };

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::chunk_t::chunk_t(T *buf, size_t s, size_t c) noexcept : _buffer(buf),
                                                                                  _size(s),
                                                                                  _capacity(c)
  {
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::storage_t::storage_t(buffer_t &owner) noexcept
      : _owner(owner),
        _chunk_write_index(0),
        _write_offset(0),
        _chunk_read_index(0),
        _read_offset(0)
  {
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::storage_t::~storage_t() noexcept
  {
    for (auto &ck : _chunks)
    {
      _owner.deallocate(ck._buffer, ck._capacity);
    }
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::buffer_t(allocator_type const &allocator) : _allocator(allocator),
                                                                      _storage(std::make_shared<storage_t>(*this))
  {
    T *buf = _allocator.allocate(default_net_buffer_size);
    if (buf == nullptr)
      throw std::bad_alloc();
    _storage->_chunks.emplace_back(buf, 0, default_net_buffer_size);
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::buffer_t(size_t size,
                                   allocator_type const &allocator) : _allocator(allocator),
                                                                      _storage(std::make_shared<storage_t>(*this))
  {
    size_t count = 1;
    for (size_t i = 0; i < count; i++)
    {
      T *buf = _allocator.allocate(size);
      if (buf == nullptr)
        throw std::bad_alloc();
      _storage->_chunks.emplace_back(buf, 0, size);
    }
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR>::buffer_t(buffer_t const &buffer) noexcept : _allocator(buffer._allocator),
                                                                             _storage(buffer._storage)

  {
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR>::buffer_t(buffer_t &&buffer) noexcept : _allocator(std::move(buffer._allocator)),
                                                                        _storage(std::move(buffer._storage))
  {
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR> &buffer_t<T, ALLOCATOR>::operator=(buffer_t const &buffer) noexcept
  {
    // don't change _allocator, http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0337r0.html
    _storage = buffer._storage;
    return *this;
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR> &buffer_t<T, ALLOCATOR>::operator=(buffer_t &&buffer) noexcept
  {
    // don't change _allocator, http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0337r0.html
    _storage = std::move(buffer._storage);
    return *this;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::append(std::string_view const &s)
  {
    if (s.length() == 0)
      return;

    const char *p = s.data();
    const char *end = s.data() + s.length();
    size_t i = _storage->_chunk_write_index;
    size_t offset = _storage->_write_offset;
    for (; i < _storage->_chunks.size() && p != end; i++)
    {
      T *buf = _storage->_chunks[i]._buffer + offset;
      size_t cs = mrpc::min_v(_storage->_chunks[i]._capacity - offset,
                              static_cast<size_t>(end - p));
      memcpy(buf, p, cs);
      p += cs;
      _storage->_chunks[i]._size += cs;
      offset = 0;
    }

    if (p != end)
    {
      size_t rs = end - p;
      T *buf = _allocator.allocate(rs);
      if (buf == nullptr)
        throw std::bad_alloc();
      memcpy(buf, p, rs);
      _storage->_chunks.emplace_back(buf, rs, rs);
      i = _storage->_chunks.size() - 1;
    }

    _storage->_chunk_write_index = (i != _storage->_chunks.size() ? i : _storage->_chunks.size() - 1);
    _storage->_write_offset = _storage->_chunks[_storage->_chunk_write_index]._size;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::own_buf(T *buf, size_t len, size_t capacity)
  {
    if (buf == nullptr)
      return;
    M_ASSERT(is_no_bufs());
    _storage->_chunks.emplace_back(buf, len, capacity);
    _storage->_chunk_write_index = _storage->_chunks.size() - 1;
    _storage->_write_offset = len;
  }

  template <typename T, typename ALLOCATOR>
  template <typename BUF_ARRAY, typename FUNC>
  size_t buffer_t<T, ALLOCATOR>::get_remain_buf_for_append(BUF_ARRAY &&array, size_t len, FUNC &&func)
  {
    if (is_no_bufs())
    {
      T *buf = _allocator.allocate(default_net_buffer_size);
      if (buf == nullptr)
        throw std::bad_alloc();
      own_buf(buf, 0, default_net_buffer_size);
    }
    size_t ret = 0;
    size_t end = mrpc::min_v(_storage->_chunk_write_index + len, _storage->_chunks.size());
    for (size_t i = _storage->_chunk_write_index; i < end; i++)
    {
      if (i != _storage->_chunk_write_index)
        func(&array[i - _storage->_chunk_write_index], _storage->_chunks[i]._buffer, _storage->_chunks[i]._capacity);
      else
        func(&array[i - _storage->_chunk_write_index], _storage->_chunks[i]._buffer + _storage->_write_offset,
             _storage->_chunks[i]._capacity - _storage->_write_offset);
      ret++;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  template <typename BUF_ARRAY, typename FUNC>
  size_t buffer_t<T, ALLOCATOR>::append_remain_msg_to_array(BUF_ARRAY &&array, size_t len, FUNC &&func)
  {
    size_t ret = 0;
    size_t end = mrpc::min_v(_storage->_chunk_read_index + len, _storage->_chunk_write_index + 1);
    for (size_t i = _storage->_chunk_read_index; i < end; i++)
    {
      if (i != _storage->_chunk_read_index)
        func(&array[i - _storage->_chunk_read_index], _storage->_chunks[i]._buffer, _storage->_chunks[i]._size);
      else
        func(&array[i - _storage->_chunk_read_index], _storage->_chunks[i]._buffer + _storage->_read_offset,
             _storage->_chunks[i]._size - _storage->_read_offset);
      ret++;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::for_each(std::function<void(chunk_t &ck)> const &func)
  {
    for (auto &ck : _storage->_chunks)
    {
      func(ck);
    }
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::writer_goahead(size_t bytes) noexcept
  {
    size_t i = _storage->_chunk_write_index;
    for (; i < _storage->_chunks.size() && bytes > 0; i++)
    {
      auto &ck = _storage->_chunks[i];
      size_t can_append_size = mrpc::min_v(bytes, ck._capacity - ck._size);
      ck._size += can_append_size;
      bytes -= can_append_size;
    }
    _storage->_chunk_write_index = (i != _storage->_chunks.size() ? i : _storage->_chunks.size() - 1);
    _storage->_write_offset = _storage->_chunks[_storage->_chunk_write_index]._size;
    
    if(_storage->_write_offset == _storage->_chunks[_storage->_chunk_write_index]._capacity && 
      _storage->_chunk_write_index < _storage->_chunks.size() - 1)
    {
      _storage->_chunk_write_index++;
      _storage->_write_offset = 0;
    }
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::reader_goahead(size_t bytes) noexcept
  {
    for (;;)
    {
      auto &ck = _storage->_chunks[_storage->_chunk_read_index];
      size_t can_forward_size = mrpc::min_v(bytes, ck._size - _storage->_read_offset);
      bytes -= can_forward_size;
      if (bytes > 0)
      {
        if (_storage->_chunk_read_index == _storage->_chunk_write_index)
        {
          _storage->_read_offset += can_forward_size;
          break;
        }
        else
        {
          _storage->_chunk_read_index++;
          continue;
        }
      }
      else // bytes == 0
      {
        _storage->_read_offset += can_forward_size;
        break;
      }
    }

    if(_storage->_read_offset == _storage->_chunks[_storage->_chunk_read_index]._size && 
      _storage->_chunk_read_index < _storage->_chunk_write_index)
    {
      _storage->_chunk_read_index++;
      _storage->_read_offset = 0;
    }
  }

  template <typename T, typename ALLOCATOR>
  inline bool buffer_t<T, ALLOCATOR>::is_eof() noexcept
  {
    return _storage->_chunk_read_index == _storage->_chunk_write_index &&
           _storage->_read_offset == _storage->_write_offset;
  }

  template <typename T, typename ALLOCATOR>
  inline bool buffer_t<T, ALLOCATOR>::is_no_bufs() noexcept
  {
    return _storage->_chunk_write_index == _storage->_chunks.size() ||
           (_storage->_chunk_write_index == _storage->_chunks.size() - 1 &&
               _storage->_write_offset == _storage->_chunks[_storage->_chunk_write_index]._capacity);
  }

  template <typename T, typename ALLOCATOR>
  inline size_t buffer_t<T, ALLOCATOR>::content_size() const noexcept
  {
    size_t ret = 0;
    for (size_t i = 0; i <= _storage->_chunk_write_index; i++)
    {
      ret += _storage->_chunks[i]._size;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::clear() noexcept
  {
    for (auto &ck : _storage->_chunks)
    {
      ck._size = 0;
    }
    _storage->_chunk_write_index = 0;
    _storage->_write_offset = 0;
    _storage->_chunk_read_index = 0;
    _storage->_read_offset = 0;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::go_to_start() noexcept
  {
    _storage->_chunk_read_index = 0;
    _storage->_read_offset = 0;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::go_to_end() noexcept
  {
    _storage->_chunk_read_index = _storage->_chunk_write_index;
    _storage->_read_offset = _storage->_write_offset;
  }

  template <typename T, typename ALLOCATOR>
  inline std::string buffer_t<T, ALLOCATOR>::to_string()
  {
    std::string ret;
    for (size_t i = 0; i <= _storage->_chunk_write_index; i++)
    {
      ret.append(_storage->_chunks[i]._buffer, _storage->_chunks[i]._buffer + _storage->_chunks[i]._size);
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline std::string buffer_t<T, ALLOCATOR>::remain_string()
  {
    std::string ret;
    for (size_t i = _storage->_chunk_read_index; i <= _storage->_chunk_write_index; i++)
    {
      if (i != _storage->_chunk_read_index)
        ret.append(_storage->_chunks[i]._buffer, _storage->_chunks[i]._buffer + _storage->_chunks[i]._size);
      else
        ret.append(_storage->_chunks[i]._buffer + _storage->_read_offset, _storage->_chunks[i]._buffer + _storage->_chunks[i]._size);
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline T *buffer_t<T, ALLOCATOR>::allocate(size_t capacity)
  {
    return _allocator.allocate(capacity);
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::deallocate(T *p, size_t capacity)
  {
    _allocator.deallocate(p, capacity);
  }

} // namespace mrpc::detail

namespace mrpc
{
  using buffer_t = detail::buffer_t<byte>;
}