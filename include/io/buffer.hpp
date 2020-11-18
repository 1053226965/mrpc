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
      T *buffer_;
      size_t size_;
      size_t capacity_;
    };

    struct storage_t
    {
      storage_t(buffer_t &owner) noexcept;
      ~storage_t() noexcept;
      buffer_t &owner_;
      std::vector<chunk_t> chunks_;
      size_t chunk_write_index_;
      size_t write_offset_; // offset for chunks_[chunk_write_index_]
      size_t chunk_read_index_;
      size_t read_offset_;
    };

    buffer_t(allocator_type const &allocator = allocator_type());
    buffer_t(size_t size, allocator_type const &allocator = allocator_type());
    buffer_t(buffer_t const &buffer) noexcept;
    buffer_t(buffer_t &&buffer) noexcept;
    ~buffer_t() noexcept { storage_ = nullptr; }

    buffer_t &operator=(buffer_t &&buffer) noexcept;
    buffer_t &operator=(buffer_t const &buffer) noexcept;

    void append(std::string_view const &s);
    void own_buf(T *buf, size_t len, size_t capacity);

    template <typename BUF_ARRAY, typename FUNC> // FUNC(BUF*, T*, size_t)
    size_t get_remain_buf_for_append(BUF_ARRAY &&array, size_t len, FUNC &&func);
    template <typename BUF_ARRAY, typename FUNC> // FUNC(BUF*, T*, size_t)
    size_t append_remain_msg_to_array(BUF_ARRAY &&array, size_t len, FUNC &&func);
    void writer_goahead(size_t bytes) noexcept;
    void reader_goahead(size_t bytes) noexcept;
    void for_each(std::function<void(chunk_t &ck)> const &func);

    bool is_eof() noexcept;
    bool is_no_bufs() noexcept;
    auto &back_chunk() { return storage_->chunks_[storage_->chunk_write_index_]; }
    size_t chunks_size() const noexcept { return storage_->chunks_.size(); }
    size_t remain_chunks_size() const noexcept { return storage_->chunks_.size() - storage_->chunk_write_index_; }
    size_t content_size() const noexcept;
    void clear() noexcept;

    void go_to_start() noexcept;
    void go_to_end() noexcept;

    std::string remain_string();
    std::string to_string();

  private:
    T *allocate(size_t capacity);
    void deallocate(T *p, size_t capacity);

  private:
    allocator_type allocator_;
    std::shared_ptr<storage_t> storage_;
  };

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::chunk_t::chunk_t(T *buf, size_t s, size_t c) noexcept : buffer_(buf),
                                                                                  size_(s),
                                                                                  capacity_(c)
  {
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::storage_t::storage_t(buffer_t &owner) noexcept
      : owner_(owner),
        chunk_write_index_(0),
        write_offset_(0),
        chunk_read_index_(0),
        read_offset_(0)
  {
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::storage_t::~storage_t() noexcept
  {
    for (auto &ck : chunks_)
    {
      owner_.deallocate(ck.buffer_, ck.capacity_);
    }
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::buffer_t(allocator_type const &allocator) : allocator_(allocator),
                                                                      storage_(std::make_shared<storage_t>(*this))
  {
    T *buf = allocator_.allocate(default_net_buffer_size);
    if (buf == nullptr)
      throw std::bad_alloc();
    storage_->chunks_.emplace_back(buf, 0, default_net_buffer_size);
  }

  template <typename T, typename ALLOCATOR>
  buffer_t<T, ALLOCATOR>::buffer_t(size_t size,
                                   allocator_type const &allocator) : allocator_(allocator),
                                                                      storage_(std::make_shared<storage_t>(*this))
  {
    size_t count = 1;
    for (size_t i = 0; i < count; i++)
    {
      T *buf = allocator_.allocate(size);
      if (buf == nullptr)
        throw std::bad_alloc();
      storage_->chunks_.emplace_back(buf, 0, size);
    }
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR>::buffer_t(buffer_t const &buffer) noexcept : allocator_(buffer.allocator_),
                                                                             storage_(buffer.storage_)

  {
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR>::buffer_t(buffer_t &&buffer) noexcept : allocator_(std::move(buffer.allocator_)),
                                                                        storage_(std::move(buffer.storage_))
  {
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR> &buffer_t<T, ALLOCATOR>::operator=(buffer_t const &buffer) noexcept
  {
    // don't change allocator_, http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0337r0.html
    storage_ = buffer.storage_;
    return *this;
  }

  template <typename T, typename ALLOCATOR>
  inline buffer_t<T, ALLOCATOR> &buffer_t<T, ALLOCATOR>::operator=(buffer_t &&buffer) noexcept
  {
    // don't change allocator_, http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0337r0.html
    storage_ = std::move(buffer.storage_);
    return *this;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::append(std::string_view const &s)
  {
    if (s.length() == 0)
      return;

    const char *p = s.data();
    const char *end = s.data() + s.length();
    size_t i = storage_->chunk_write_index_;
    size_t offset = storage_->write_offset_;
    for (; i < storage_->chunks_.size() && p != end; i++)
    {
      T *buf = storage_->chunks_[i].buffer_ + offset;
      size_t cs = mrpc::min_v(storage_->chunks_[i].capacity_ - offset,
                              static_cast<size_t>(end - p));
      memcpy(buf, p, cs);
      p += cs;
      storage_->chunks_[i].size_ += cs;
      offset = 0;
    }

    if (p != end)
    {
      size_t rs = end - p;
      T *buf = allocator_.allocate(rs);
      if (buf == nullptr)
        throw std::bad_alloc();
      memcpy(buf, p, rs);
      storage_->chunks_.emplace_back(buf, rs, rs);
      i = storage_->chunks_.size() - 1;
    }

    storage_->chunk_write_index_ = (i != storage_->chunks_.size() ? i : storage_->chunks_.size() - 1);
    storage_->write_offset_ = storage_->chunks_[storage_->chunk_write_index_].size_;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::own_buf(T *buf, size_t len, size_t capacity)
  {
    if (buf == nullptr)
      return;
    M_ASSERT(is_no_bufs());
    storage_->chunks_.emplace_back(buf, len, capacity);
    storage_->chunk_write_index_ = storage_->chunks_.size() - 1;
    storage_->write_offset_ = len;
  }

  template <typename T, typename ALLOCATOR>
  template <typename BUF_ARRAY, typename FUNC>
  size_t buffer_t<T, ALLOCATOR>::get_remain_buf_for_append(BUF_ARRAY &&array, size_t len, FUNC &&func)
  {
    if (is_no_bufs())
    {
      T *buf = allocator_.allocate(default_net_buffer_size);
      if (buf == nullptr)
        throw std::bad_alloc();
      own_buf(buf, 0, default_net_buffer_size);
    }
    size_t ret = 0;
    size_t end = mrpc::min_v(storage_->chunk_write_index_ + len, storage_->chunks_.size());
    for (size_t i = storage_->chunk_write_index_; i < end; i++)
    {
      if (i != storage_->chunk_write_index_)
        func(&array[i - storage_->chunk_write_index_], storage_->chunks_[i].buffer_, storage_->chunks_[i].capacity_);
      else
        func(&array[i - storage_->chunk_write_index_], storage_->chunks_[i].buffer_ + storage_->write_offset_,
             storage_->chunks_[i].capacity_ - storage_->write_offset_);
      ret++;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  template <typename BUF_ARRAY, typename FUNC>
  size_t buffer_t<T, ALLOCATOR>::append_remain_msg_to_array(BUF_ARRAY &&array, size_t len, FUNC &&func)
  {
    size_t ret = 0;
    size_t end = mrpc::min_v(storage_->chunk_read_index_ + len, storage_->chunk_write_index_ + 1);
    for (size_t i = storage_->chunk_read_index_; i < end; i++)
    {
      if (i != storage_->chunk_read_index_)
        func(&array[i - storage_->chunk_read_index_], storage_->chunks_[i].buffer_, storage_->chunks_[i].size_);
      else
        func(&array[i - storage_->chunk_read_index_], storage_->chunks_[i].buffer_ + storage_->read_offset_,
             storage_->chunks_[i].size_ - storage_->read_offset_);
      ret++;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::for_each(std::function<void(chunk_t &ck)> const &func)
  {
    for (auto &ck : storage_->chunks_)
    {
      func(ck);
    }
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::writer_goahead(size_t bytes) noexcept
  {
    size_t i = storage_->chunk_write_index_;
    for (; i < storage_->chunks_.size() && bytes > 0; i++)
    {
      auto &ck = storage_->chunks_[i];
      size_t can_append_size = mrpc::min_v(bytes, ck.capacity_ - ck.size_);
      ck.size_ += can_append_size;
      bytes -= can_append_size;
    }
    storage_->chunk_write_index_ = (i != storage_->chunks_.size() ? i : storage_->chunks_.size() - 1);
    storage_->write_offset_ = storage_->chunks_[storage_->chunk_write_index_].size_;
    
    if(storage_->write_offset_ == storage_->chunks_[storage_->chunk_write_index_].capacity_ && 
      storage_->chunk_write_index_ < storage_->chunks_.size() - 1)
    {
      storage_->chunk_write_index_++;
      storage_->write_offset_ = 0;
    }
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::reader_goahead(size_t bytes) noexcept
  {
    for (;;)
    {
      auto &ck = storage_->chunks_[storage_->chunk_read_index_];
      size_t can_forward_size = mrpc::min_v(bytes, ck.size_ - storage_->read_offset_);
      bytes -= can_forward_size;
      if (bytes > 0)
      {
        if (storage_->chunk_read_index_ == storage_->chunk_write_index_)
        {
          storage_->read_offset_ += can_forward_size;
          break;
        }
        else
        {
          storage_->chunk_read_index_++;
          continue;
        }
      }
      else // bytes == 0
      {
        storage_->read_offset_ += can_forward_size;
        break;
      }
    }

    if(storage_->read_offset_ == storage_->chunks_[storage_->chunk_read_index_].size_ && 
      storage_->chunk_read_index_ < storage_->chunk_write_index_)
    {
      storage_->chunk_read_index_++;
      storage_->read_offset_ = 0;
    }
  }

  template <typename T, typename ALLOCATOR>
  inline bool buffer_t<T, ALLOCATOR>::is_eof() noexcept
  {
    return storage_->chunk_read_index_ == storage_->chunk_write_index_ &&
           storage_->read_offset_ == storage_->write_offset_;
  }

  template <typename T, typename ALLOCATOR>
  inline bool buffer_t<T, ALLOCATOR>::is_no_bufs() noexcept
  {
    return storage_->chunk_write_index_ == storage_->chunks_.size() ||
           (storage_->chunk_write_index_ == storage_->chunks_.size() - 1 &&
               storage_->write_offset_ == storage_->chunks_[storage_->chunk_write_index_].capacity_);
  }

  template <typename T, typename ALLOCATOR>
  inline size_t buffer_t<T, ALLOCATOR>::content_size() const noexcept
  {
    size_t ret = 0;
    for (size_t i = 0; i <= storage_->chunk_write_index_; i++)
    {
      ret += storage_->chunks_[i].size_;
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::clear() noexcept
  {
    for (auto &ck : storage_->chunks_)
    {
      ck.size_ = 0;
    }
    storage_->chunk_write_index_ = 0;
    storage_->write_offset_ = 0;
    storage_->chunk_read_index_ = 0;
    storage_->read_offset_ = 0;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::go_to_start() noexcept
  {
    storage_->chunk_read_index_ = 0;
    storage_->read_offset_ = 0;
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::go_to_end() noexcept
  {
    storage_->chunk_read_index_ = storage_->chunk_write_index_;
    storage_->read_offset_ = storage_->write_offset_;
  }

  template <typename T, typename ALLOCATOR>
  inline std::string buffer_t<T, ALLOCATOR>::to_string()
  {
    std::string ret;
    for (size_t i = 0; i <= storage_->chunk_write_index_; i++)
    {
      ret.append(storage_->chunks_[i].buffer_, storage_->chunks_[i].buffer_ + storage_->chunks_[i].size_);
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline std::string buffer_t<T, ALLOCATOR>::remain_string()
  {
    std::string ret;
    for (size_t i = storage_->chunk_read_index_; i <= storage_->chunk_write_index_; i++)
    {
      if (i != storage_->chunk_read_index_)
        ret.append(storage_->chunks_[i].buffer_, storage_->chunks_[i].buffer_ + storage_->chunks_[i].size_);
      else
        ret.append(storage_->chunks_[i].buffer_ + storage_->read_offset_, storage_->chunks_[i].buffer_ + storage_->chunks_[i].size_);
    }
    return ret;
  }

  template <typename T, typename ALLOCATOR>
  inline T *buffer_t<T, ALLOCATOR>::allocate(size_t capacity)
  {
    return allocator_.allocate(capacity);
  }

  template <typename T, typename ALLOCATOR>
  inline void buffer_t<T, ALLOCATOR>::deallocate(T *p, size_t capacity)
  {
    allocator_.deallocate(p, capacity);
  }

} // namespace mrpc::detail

namespace mrpc
{
  using buffer_t = detail::buffer_t<byte>;
}