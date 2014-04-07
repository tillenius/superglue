#ifndef SG_TASKQUEUE_HPP_INCLUDED
#define SG_TASKQUEUE_HPP_INCLUDED

#include "sg/core/taskqueueunsafe.hpp"
#include "sg/core/taskqueuesafe.hpp"

#include <stdint.h>

namespace sg {

template<typename Options> class TaskBase;

template <typename Options>
class TaskQueueDefault
 : public detail::TaskQueueSafe< detail::TaskQueueDefaultUnsafe<Options>, detail::QueueSpinLocked> {};

} // namespace sg

#endif // SG_TASKQUEUE_HPP_INCLUDED
