#ifndef SG_DEPCHECK_LAZY_HPP_INCLUDED
#define SG_DEPCHECK_LAZY_HPP_INCLUDED

namespace sg {

template <typename Options> class TaskBase;
template<typename Options>
struct LazyDependencyChecking {
    typedef typename Options::WaitListType WaitListType;
    typedef typename WaitListType::unsafe_t TaskQueueUnsafe;

    static bool check_before_execution(TaskBase<Options> *task) {
        return task->are_dependencies_solved_or_notify();
    }

    static bool check_at_submit(TaskBase<Options> *task) {
        return true;
    }

    static void check_at_wakeup(TaskQueueUnsafe &list) {}
};

} // namespace sg

#endif // SG_DEPCHECK_LAZY_HPP_INCLUDED
