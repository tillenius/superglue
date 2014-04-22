#ifndef SG_SUPERGLUEBASE_HPP_INCLUDED
#define SG_SUPERGLUEBASE_HPP_INCLUDED

#include "sg/core/barrierprotocol.hpp"

#include <cassert>

namespace sg {

template <typename Options> class TaskBase;
template <typename Options> class HandleBase;
template <typename Options> class SuperGlue;
template <typename Options> class AccessUtil;

namespace detail {

// ===========================================================================
// Option PauseExecution
// ===========================================================================
template<typename Options, typename T = typename Options::PauseExecution> class SuperGlue_PauseExecution;

template<typename Options>
class SuperGlue_PauseExecution<Options, typename Options::Disable> {};

template<typename Options>
class SuperGlue_PauseExecution<Options, typename Options::Enable> {
public:
    void start_executing() {
        SuperGlue<Options> *this_(static_cast<SuperGlue<Options> *>(this));
        this_->tman->start_executing();
    }
};

// ===========================================================================
// CheckLockableRequired -- check that no access types commutes and required
// exclusive access if lockable is disabled
// ===========================================================================
template<typename Options, typename T = typename Options::Lockable>
struct CheckLockableRequired {
    enum { access_types_needs_lockable = 1 };
};

template<typename Options>
struct CheckLockableRequired<Options, typename Options::Disable> {
    template<typename T>
    struct NeedsLockablePredicate {
        enum { result = ((T::exclusive == 1) && (T::commutative == 1)) ? 1 : 0 };
    };

    typedef AccessUtil<Options> AU;
    typedef typename AU::template AnyType<NeedsLockablePredicate> NeedsLock;

    enum { access_types_needs_lockable = NeedsLock::result ? 0 : 1};
};

template<bool> struct STATIC_ASSERT {};
template<> struct STATIC_ASSERT<true> { typedef struct {} type; };

template<typename Options>
struct SANITY_CHECKS {

    template<typename T>
    struct is_unsigned {
        enum { value = T(-1) > T(0) ? 1 : 0 };
    };

    // version_t must be an unsigned type
    typedef typename STATIC_ASSERT< is_unsigned<typename Options::version_t>::value >::type check_version_t;

    // check that Lockable isn't disabled when access types require it to be enabled
    typedef typename STATIC_ASSERT< CheckLockableRequired<Options>::access_types_needs_lockable >::type check_lockable;
};

} // namespace detail

// ===========================================================================
// SuperGlue
// ===========================================================================
template<typename Options>
class SuperGlue
  : public detail::SuperGlue_PauseExecution<Options>,
    public Options::ThreadAffinity,
    private detail::SANITY_CHECKS<Options>
{
    template<typename, typename> friend class SuperGlue_PauseExecution;
    typedef typename Options::ReadyListType TaskQueue;
    typedef typename Options::ThreadingManagerType ThreadingManager;
    typedef typename TaskQueue::unsafe_t TaskQueueUnsafe;

private:
    SuperGlue(const SuperGlue &);
    SuperGlue &operator=(const SuperGlue &);

    bool delete_threadmanager;

public:
    ThreadingManager *tman;
    TaskExecutor<Options> *main_task_executor;
    char padding0[Options::CACHE_LINE_SIZE];

    int next_queue;
    char padding2[Options::CACHE_LINE_SIZE];

public:
    SuperGlue(ThreadingManager &tman_)
     : delete_threadmanager(false), tman(&tman_), next_queue(0) {
        tman->init();
        main_task_executor = tman->get_worker(0);
     }

    SuperGlue(int req = -1)
     : delete_threadmanager(true), tman(new ThreadingManager(req)), next_queue(0) {
        main_task_executor = tman->get_worker(0);
    }

    ~SuperGlue() {
        tman->stop();
        if (delete_threadmanager)
            delete tman;
    }

    int get_num_cpus() { return tman->get_num_cpus(); }

    // USER INTERFACE {

    void submit(TaskBase<Options> *task) {
        submit(task, next_queue);
        // data race here when multiple threads submit tasks, but it
        // is not important that the distribution is perfectly even.
        next_queue = (next_queue + 1) % get_num_cpus();
    }

    void submit(TaskBase<Options> *task, int cpuid) {
        tman->get_worker(cpuid)->submit(task);
    }

    void barrier() {
        tman->barrier_protocol.barrier(*main_task_executor);
    }

    void wait(HandleBase<Options> &handle) {
        TaskQueueUnsafe woken;
        while (handle.next_version()-1 != handle.get_current_version()) {
            main_task_executor->execute_tasks(woken);
            Atomic::compiler_fence(); // to reload the handle versions
        }
        if (!woken.empty())
            main_task_executor->push_front_list(woken);
    }

    // }
};

} // namespace sg

#endif // SG_SUPERGLUEBASE_HPP_INCLUDED
