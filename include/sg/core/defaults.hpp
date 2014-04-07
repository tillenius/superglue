#ifndef SG_DEFAULTS_HPP_INCLUDED
#define SG_DEFAULTS_HPP_INCLUDED

#include "sg/core/access_rwa.hpp"
#include "sg/platform/affinity.hpp"
#include <memory>

#if defined(_OPENMP)
#include "sg/option/threadingmanager_omp.hpp"
#else
#include "sg/option/threadingmanager_default.hpp"
#endif

namespace sg {

template<typename Options> class SuperGlue;
template<typename Options> class SuperGlueBase;
template<typename Options> class TaskExecutor;
template<typename Options> class TaskExecutorBase;
template<typename Options> class TaskBase;
template<typename Options> class TaskBaseDefault;
template<typename, typename, int> class TaskAccessMixin;
template<typename Options> class Handle;
template<typename Options> class HandleBase;
template<typename Options> class TaskQueueDefault;
template<typename Options> class TaskQueueDefaultUnsafe;

// ============================================================================
// Default Steal Order:  Start from random queue and search upwards
// ============================================================================
template<typename Options>
class DefaultStealOrder {
    typedef typename Options::ReadyListType TaskQueue;
private:
    size_t seed;

public:
    void init(TaskExecutor<Options> *parent) {
        seed = (size_t) parent->id;
    }

    bool steal(typename Options::ThreadingManagerType &tman, size_t id, TaskBase<Options> *&dest) {
        TaskQueue **taskQueues(tman.get_task_queues());
        const size_t num_queues(tman.get_num_cpus());
        seed = seed * 1664525 + 1013904223;
        const size_t random(seed % num_queues);

        for (size_t i = random; i < num_queues; ++i) {
            if (i == id)
                continue;
            if (taskQueues[i]->pop_back(dest))
                return true;
        }
        for (size_t i = 0; i < random; ++i) {
            if (i == id)
                continue;
            if (taskQueues[i]->pop_back(dest))
                return true;
        }
        return false;
    }
};

// ============================================================================
// Default Dependency checking: Check at submit
// ============================================================================
template<typename Options>
struct EagerDependencyChecking {
    typedef typename Options::WaitListType WaitListType;
    typedef typename WaitListType::unsafe_t TaskQueueUnsafe;

    struct DependenciesNotSolvedPredicate {
        bool operator()(TaskBase<Options> *elem) {
            return !elem->are_dependencies_solved_or_notify();
        }
    };

    static bool check_before_execution(TaskBase<Options> *) {
        return true;
    }
    static bool check_at_submit(TaskBase<Options> *task) {
        return task->are_dependencies_solved_or_notify();
    }
    static void check_at_wakeup(TaskQueueUnsafe &list) {
        list.erase_if(DependenciesNotSolvedPredicate());
    }
};

// ============================================================================
// Default Instrumentation: None
// One object instantiated per thread.
// init() and finalize() must be static and are called once per threadmanager.
// use constructor and destructor for thread-wise init() and finalize()
// ============================================================================
template<typename Options>
struct NoInstrumentation {
    NoInstrumentation(int threadid) {}
    static void init() {}
    static void finalize() {}

    static void run_task_before(TaskBase<Options> *) {}
    static void run_task_after(TaskBase<Options> *) {}
};

// ============================================================================
// Default DAG Creation: None
// ============================================================================
template<typename Options>
struct NoDAG {
    typedef typename Options::version_t version_t;
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename AccessInfo::Type AccessType;
    
    static void add_dependency(TaskBase<Options> *task, 
                               Handle<Options> *handle,
                               version_t version,
                               AccessType type) {}
    static void task_finish(TaskBase<Options> *task,
                            Handle<Options> *handle,
                            version_t version) {}
    static void init() {}
    static void finalize() {}

    static void run_task_before(TaskBase<Options> *) {}
    static void run_task_after(TaskBase<Options> *) {}
};

// ============================================================================
// Default Task Deletion: None
// ============================================================================
template<typename Options>
struct DeleteTaskDefault {
    static void free(TaskBase<Options> *task) {
        delete task;
    }
};

// ============================================================================
// Default Thread Affinity
// ============================================================================
template<typename Options>
struct DefaultThreadAffinity {
    static void init() {}
    static void pin_main_thread() {
        affinity_cpu_set cpu_set;
        cpu_set.set(0);
        ThreadAffinity::set_affinity(cpu_set);
    }
    static void pin_workerthread(int id) {
        affinity_cpu_set cpu_set;
        cpu_set.set(id);
        ThreadAffinity::set_affinity(cpu_set);
    }
};

// ============================================================================
// Default Options
// ============================================================================
template <typename Options>
struct DefaultOptions {

    enum { CACHE_LINE_SIZE = 64 };

    struct Enable {};
    struct Disable {};

    template<typename T2> struct Alloc {
        typedef std::allocator<T2> type;
    };
    typedef DefaultStealOrder<Options> StealOrder;
    typedef ReadWriteAdd AccessInfoType;
    typedef unsigned int version_t;
    typedef unsigned int handleid_t;
    typedef unsigned int taskid_t;
    typedef unsigned int lockcount_t;

    // Types that can be overloaded
    typedef HandleBase<Options> HandleType;
    typedef TaskBaseDefault<Options> TaskBaseType;
    template<int N> struct TaskType {
        typedef TaskAccessMixin<Options, TaskBase<Options>, N> type;
    };
    typedef TaskExecutorBase<Options> TaskExecutorType;
    typedef TaskQueueDefault<Options> WaitListType;
    typedef TaskQueueDefault<Options> ReadyListType;

    // Features
    typedef Disable TaskName;            // tasks must implement a get_name() method
    typedef Disable TaskId;              // tasks have a get_global_id() method
    typedef Disable HandleName;          // handles have set_name() and get_name() methods
    typedef Disable HandleId;            // handels have a get_global_id() method
    typedef Enable Lockable;             // handles can be locked
    typedef Enable ThreadSafeSubmit;     // submitting tasks is thread safe
    typedef Disable Subtasks;            // TaskExecutor has subtask() method
    typedef Disable PassTaskExecutor;    // the Task::run() method is called with the current TaskExecutor as parameter
    typedef Disable ThreadWorkspace;     // TaskExecutor have a getThreadWorkspace() method
    typedef Disable PauseExecution;      // No tasks are executed until setMayExecute(true) is called
    typedef Enable Stealing;             // Task stealing enabled
    typedef Disable Contributions;       // Run tasks but write to temp storage if output handle is busy

    // Size of ThreadWorkspace (only used if ThreadWorkspace is enabled)
    enum { ThreadWorkspace_size = 102400 };

    // Dependency Checking Options
    typedef EagerDependencyChecking<Options> DependencyChecking;

    // Instrumentation
    typedef NoInstrumentation<Options> Instrumentation;
    
    // DAG Creation
    typedef NoDAG<Options> LogDAG;

    // Releasing task memory
    typedef DeleteTaskDefault<Options> FreeTask;

    // Thread Affinity
    typedef DefaultThreadAffinity<Options> ThreadAffinity;

#if defined(_OPENMP)
    typedef ThreadingManagerOMP<Options> ThreadingManagerType;
#else
    typedef ThreadingManagerDefault<Options> ThreadingManagerType;
#endif

};

} // namespace sg

#endif // SG_DEFAULTS_HPP_INCLUDED
