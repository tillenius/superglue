#ifndef SG_TYPES_HPP_INCLUDED
#define SG_TYPES_HPP_INCLUDED

#include <vector>
#include <deque>

namespace sg {

template<typename Options>
struct Types {
    template<typename T>
    struct vector_t {
        typedef typename std::vector<T, typename Options::template Alloc<T>::type > type;
    };
    template<typename T>
    struct deque_t {
        typedef typename std::deque<T, typename Options::template Alloc<T>::type > type;
    };

};

} // namespace sg

#endif // SG_TYPES_HPP_INCLUDED
