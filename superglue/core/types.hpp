#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include "orderedvec.hpp"
#include <vector>
#include <deque>

template<typename Options> class TaskBase;
template<typename Options, typename T = typename Options::ListQueue> class TaskQueueUnsafe;

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

    typedef typename Options::version_t version_t;

    typedef ordered_vec_t< typename deque_t< elem_t<version_t, TaskQueueUnsafe<Options> > >::type, version_t, TaskQueueUnsafe<Options> > versionmap_t;
    typedef typename deque_t<TaskBase<Options> *>::type taskdeque_t;
};

#endif // __TYPES_HPP__
