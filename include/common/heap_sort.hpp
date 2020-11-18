#pragma once
#include <vector>
#include <memory>

namespace ds
{
  namespace detail
  {
    /* left_child = parent * 2 + 1, right_child = parent * 2 + 2 */
    template<typename T,
      typename CMP = std::less<>,
      typename ALLOCATOR = std::allocator<T>>
    class heap_sort_t
    {
    public:
      using value_type = T;
      using cmp_type = CMP;
      using allocator_type = ALLOCATOR;
      using rmove_reference_type = std::remove_reference_t<T>;

      heap_sort_t();

      template<typename VT,
        typename = std::enable_if<std::is_convertible_v<VT, T>>>
      void add(VT&& v);

      template<typename ARRAY>
      void add_array(ARRAY&& vs);

      bool empty() const noexcept { return array_.empty(); }
      T const& top() const noexcept { return array_[0]; }
      T pop() noexcept;

    private:
      size_t left_child_index(size_t i) noexcept { return (i << 1) + 1; }
      size_t right_child_index(size_t i) noexcept { return (i << 1) + 2; }
      size_t parent_index(size_t i) noexcept { return (i - 1) >> 1; }


      std::vector<T, ALLOCATOR> array_;
    };


    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    inline heap_sort_t<T, CMP, ALLOCATOR>::heap_sort_t() :
      array_()
    {
    }

    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    template<typename VT, typename>
    inline void heap_sort_t<T, CMP, ALLOCATOR>::add(VT&& v)
    {
      array_.push_back(std::forward<VT>(v));
      size_t index = array_.size() - 1;
      for(size_t pindex = parent_index(index); 
        index > 0 && cmp_type()(array_[index], array_[pindex]);
        pindex = parent_index(index)) {
          std::swap(array_[index], array_[pindex]);
          index = pindex;
      }
    }
    
    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    template<typename ARRAY>
    inline void heap_sort_t<T, CMP, ALLOCATOR>::add_array(ARRAY&& vs)
    {
      for (auto& v : vs) {
        add(static_cast<T>(v));
      }
    }

    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    inline T heap_sort_t<T, CMP, ALLOCATOR>::pop() noexcept
    {
      std::swap(array_[0], array_.back());
      rmove_reference_type ret = std::move(array_.back());
      array_.pop_back();

      size_t index = 0;
      while (index < array_.size()) {
        size_t li = left_child_index(index);
        if (li >= array_.size()) break;
        size_t ri = right_child_index(index);
        if (ri < array_.size()) {
          if (cmp_type()(array_[li], array_[ri])) {
            std::swap(array_[index], array_[li]);
            index = li;
          }
          else {
            std::swap(array_[index], array_[ri]);
            index = ri;
          }
        }
        else if(cmp_type()(array_[li], array_[index])) {
          std::swap(array_[index], array_[li]);
          index = li;
        }
        else {
          break;
        }
      }

      return std::move(ret);
    }
  }

  template<typename T,
    typename CMP = std::less<>,
    typename ALLOCATOR = std::allocator<T>>
  using heap_sort_t = detail::heap_sort_t<T, CMP, ALLOCATOR>;
}