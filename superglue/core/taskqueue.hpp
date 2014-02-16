#ifndef __TASKQUEUE_HPP__
#define __TASKQUEUE_HPP__

#include "core/taskqueueunsafe.hpp"
#include "core/taskqueuesafe.hpp"

#include <stdint.h>
template<typename Options> class TaskBase;

template <typename Options>
class TaskQueueDefault : public detail::TaskQueueSafe<
    detail::TaskQueueDefaultUnsafe<Options>, detail::QueueSpinLocked> {};


#endif // __TASKQUEUE_HPP__
