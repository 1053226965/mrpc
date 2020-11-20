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

      bool empty() const noexcept { return _array.empty(); }
      T const& top() const noexcept { return _array[0]; }
      T pop() noexcept;

    private:
      size_t left_child_index(size_t i) noexcept { return (i << 1) + 1; }
      size_t right_child_index(size_t i) noexcept { return (i << 1) + 2; }
      size_t parent_index(size_t i) noexcept { return (i - 1) >> 1; }


      std::vector<T, ALLOCATOR> _array;
    };


    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    inline heap_sort_t<T, CMP, ALLOCATOR>::heap_sort_t() :
      _array()
    {
    }

    template<typename T,
      typename CMP,
      typename ALLOCATOR>
    template<typename VT, typename>
    inline void heap_sort_t<T, CMP, ALLOCATOR>::add(VT&& v)
    {
      _array.push_back(std::forward<VT>(v));
      size_t index = _array.size() - 1;
      for(size_t pindex = parent_index(index); 
        index > 0 && cmp_type()(_array[index], _array[pindex]);
        pindex = parent_index(index)) {
          std::swap(_array[index], _array[pindex]);
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
      std::swap(_array[0], _array.back());
      rmove_reference_type ret = std::move(_array.back());
      _array.pop_back();

      size_t index = 0;
      while (index < _array.size()) {
        size_t li = left_child_index(index);
        if (li >= _array.size()) break;
        size_t ri = right_child_index(index);
        if (ri < _array.size()) {
          if (cmp_type()(_array[li], _array[ri])) {
            std::swap(_array[index], _array[li]);
            index = li;
          }
          else {
            std::swap(_array[index], _array[ri]);
            index = ri;
          }
        }
        else if(cmp_type()(_array[li], _array[index])) {
          std::swap(_array[index], _array[li]);
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