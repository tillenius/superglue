#ifndef __TASK_HPP__
#define __TASK_HPP__

#include "core/log_dag.hpp"
#include "core/types.hpp"
#include "core/accessutil.hpp"
#include "core/access.hpp"
#include "platform/atomic.hpp"
#include <string>
#include <stdint.h>

template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;

namespace detail {

// ============================================================================
// Option: StealableFlag
// ============================================================================
template<typename Options, typename T = typename Options::TaskStealableFlag> struct Task_StealableFlag;

template<typename Options>
struct Task_StealableFlag<Options, typename Options::Disable> {};

template<typename Options>
struct Task_StealableFlag<Options, typename Options::Enable> {
    virtual bool isStealable() const = 0;
};

// ============================================================================
// Option: TaskPriorities
// ============================================================================
template<typename Options, typename T = typename Options::TaskPriorities> struct Task_Priorities;

template<typename Options>
struct Task_Priorities<Options, typename Options::Disable> {};

template<typename Options>
struct Task_Priorities<Options, typename Options::Enable> {
    virtual int getPriority() const = 0;
};

// ============================================================================
// Option: Global ID for each task
// ============================================================================
template<typename Options, typename T = typename Options::TaskId> class Task_GlobalId;

template<typename Options>
class Task_GlobalId<Options, typename Options::Disable> {};

template<typename Options>
class Task_GlobalId<Options, typename Options::Enable> {
private:
    size_t id;
public:
    Task_GlobalId() {
        static size_t global_task_id = 0;
        id = Atomic::increase_nv(&global_task_id);
    }
    size_t getGlobalId() const { return id; }
};

// ============================================================================
// Option Contributions
// ============================================================================
template<typename Options, typename T = typename Options::Contributions> class Task_Contributions;

template<typename Options>
class Task_Contributions<Options, typename Options::Disable> {
public:
    static bool canRunWithContribs() { return false; }
};
template<typename Options>
class Task_Contributions<Options, typename Options::Enable> {
public:
    virtual bool canRunWithContribs() { return false; }
};

// ============================================================================
// Option PassTaskExecutor
// ============================================================================
template<typename Options, typename T = typename Options::PassTaskExecutor> struct Task_PassTaskExecutor;

template<typename Options>
struct Task_PassTaskExecutor<Options, typename Options::Disable> {
    virtual void run() = 0;
};

template<typename Options>
struct Task_PassTaskExecutor<Options, typename Options::Enable> {
    virtual void run(TaskExecutor<Options> &) = 0;
};

// ============================================================================
// Option TaskName
// ============================================================================
template<typename Options, typename T = typename Options::TaskName> struct Task_TaskName;

template<typename Options>
struct Task_TaskName<Options, typename Options::Disable> {};

template<typename Options>
struct Task_TaskName<Options, typename Options::Enable> {
    virtual std::string getName() = 0;
};

// ============================================================================
// Option StolenFlag
// ============================================================================
template<typename Options, typename T = typename Options::TaskStolenFlag> class Task_StolenFlag;

template<typename Options>
class Task_StolenFlag<Options, typename Options::Disable> {};

template<typename Options>
class Task_StolenFlag<Options, typename Options::Enable> {
private:
    bool stolen;

public:
    Task_StolenFlag() : stolen(false) {}
    void setStolen(bool isStolen) { stolen = isStolen; }
    bool isStolen() const { return stolen; }
};

// ============================================================================
// Option ListQueue
// ============================================================================
template<typename Options, typename T = typename Options::ListQueue> class Task_ListQueue;

template<typename Options>
class Task_ListQueue<Options, typename Options::Disable> {};

template<typename Options>
class Task_ListQueue<Options, typename Options::Enable> {
public:
    uintptr_t nextPrev;
    Task_ListQueue() : nextPrev(0) {}
};

// ============================================================================
// Option Subtasks
// ============================================================================
template<typename Options, typename T = typename Options::Subtasks> class Task_Subtasks;

template<typename Options>
class Task_Subtasks<Options, typename Options::Disable> {};

template<typename Options>
class Task_Subtasks<Options, typename Options::Enable> {
public:
    size_t subtask_count;
    TaskBase<Options> *parent;
    Task_Subtasks() : subtask_count(0), parent(NULL) {}
};

} // namespace detail

// ============================================================================
// TaskBaseDefault : Base for tasks without dependencies
// ============================================================================
template<typename Options>
class TaskBaseDefault
  : public detail::Task_ListQueue<Options>,
    public detail::Task_PassTaskExecutor<Options>,
    public detail::Task_GlobalId<Options>,
    public detail::Task_StolenFlag<Options>,
    public detail::Task_TaskName<Options>,
    public detail::Task_Priorities<Options>,
    public detail::Task_StealableFlag<Options>,
    public detail::Task_Contributions<Options>,
    public detail::Task_Subtasks<Options>
{
    template<typename, typename> friend class Task_PassThreadId;
    template<typename, typename> friend struct Task_AccessData;

protected:
    size_t numAccess;
    size_t accessIdx;
    Access<Options> *accessPtr;

    TaskBaseDefault(size_t numAccess_, Access<Options> *accessPtr_)
     : numAccess(numAccess_), accessIdx(0), accessPtr(accessPtr_) {}

public:
    TaskBaseDefault() : numAccess(0), accessIdx(0), accessPtr(0) {}
    virtual ~TaskBaseDefault() {}

    size_t getNumAccess() const { return numAccess; }
    Access<Options> *getAccess() const { return accessPtr; }
    Access<Options> &getAccess(size_t i) const { return accessPtr[i]; }
    bool areDependenciesSolvedOrNotify() {
        TaskBase<Options> *this_(static_cast<TaskBase<Options> *>(this));

        const size_t numAccess = this->getNumAccess();
        for (; accessIdx < numAccess; ++accessIdx) {

            if (!accessPtr[accessIdx].getHandle()->isVersionAvailableOrNotify(this_, accessPtr[accessIdx].requiredVersion)) {
                ++accessIdx; // We consider this dependency fulfilled now, as it will be when we get the callback.
                return false;
            }
        }
        return true;
    }
};

// export Options::TaskBaseType as TaskBase (default: TaskBaseDefault<Options>)
template<typename Options> class TaskBase : public Options::TaskBaseType {};

// ============================================================================
// TaskDefault : adds registerAccess()
// ============================================================================
template<typename Options, int N = -1>
class TaskDefault : public TaskBase<Options> {
    Access<Options> access[N];
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename AccessInfo::Type AccessType;
    typedef typename Options::version_t version_t;
public:
    TaskDefault() {
        // If this assignment is done through a constructor, we can save the initial assignment,
        // but then any user-overloaded TaskBaseDefault() class must forward both constructors.
        TaskBase<Options>::accessPtr = &access[0];
    }
    void addAccess(AccessType type, Handle<Options> *handle, version_t version) {
        Access<Options> &a(access[TaskBase<Options>::numAccess]);
        a.handle = handle;
        a.requiredVersion = version;
        if (AccessUtil<Options>::needsLock(type))
            a.setNeedsLock(true);
        Log_DAG<Options>::dag_addDependency(static_cast<TaskBase<Options> *>(this), handle, version, type);
        ++TaskBase<Options>::numAccess;
    }
    void registerAccess(AccessType type, Handle<Options> *handle) {
        addAccess(type, handle, handle->schedule(type));
    }
};

// Specialization for zero dependencies
template<typename Options>
class TaskDefault<Options, 0> : public TaskBase<Options> {};

// Specialization for variable number of dependencies
template<typename Options>
class TaskDefault<Options, -1> : public TaskBase<Options> {
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename AccessInfo::Type AccessType;
    typedef typename Types<Options>::template vector_t< Access<Options> >::type access_vector_t;
    typedef typename Options::version_t version_t;
protected:
    access_vector_t access;
public:
    void addAccess(AccessType type, Handle<Options> *handle, version_t version) {
        access.push_back(Access<Options>(handle, version));
        Access<Options> &a(access[access.size()-1]);
        if (AccessUtil<Options>::needsLock(type))
            a.setNeedsLock(true);
        Log_DAG<Options>::dag_addDependency(static_cast<TaskBase<Options> *>(this), handle, version, type);
        ++TaskBase<Options>::numAccess;
        TaskBase<Options>::accessPtr = &access[0]; // vector may be reallocated at any add
    }

    void registerAccess(AccessType type, Handle<Options> *handle) {
        addAccess(type, handle, handle->schedule(type));
    }
};

// export "Options::TaskType<>::type" (default: TaskDefault) as type Task
template<typename Options, int N = -1> class Task
 : public Options::template TaskType<N>::type {};

#endif // __TASK_HPP__
