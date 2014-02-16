#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include <vector>
#include <deque>

template<typename Options> class TaskBase;

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

#endif // __TYPES_HPP__
