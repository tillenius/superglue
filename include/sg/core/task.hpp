#ifndef SG_TASK_HPP_INCLUDED
#define SG_TASK_HPP_INCLUDED

#include "sg/core/types.hpp"
#include "sg/platform/atomic.hpp"
#include <string>
#include <stdint.h>

namespace sg {

template<typename Options> class Access;
template<typename Options> class AccessUtil;
template<typename Options> class Handle;
template<typename Options> class Resource;
template<typename Options> class TaskBase;
template<typename Options> class TaskExecutor;

namespace detail {

// ============================================================================
// Option: Global ID for each task
// ============================================================================
template<typename Options, typename T = typename Options::TaskId> class Task_GlobalId;

template<typename Options>
class Task_GlobalId<Options, typename Options::Disable> {};

template<typename Options>
class Task_GlobalId<Options, typename Options::Enable> {
    typedef typename Options::taskid_t taskid_t;
private:
    taskid_t id;
public:
    Task_GlobalId() {
        static taskid_t global_task_id = 0;
        id = Atomic::increase_nv(&global_task_id);
    }
    taskid_t get_global_id() const { return id; }
};

// ============================================================================
// Option Contributions
// ============================================================================
template<typename Options, typename T = typename Options::Contributions> class Task_Contributions;

template<typename Options>
class Task_Contributions<Options, typename Options::Disable> {
public:
    static bool can_run_with_contribs() { return false; }
};
template<typename Options>
class Task_Contributions<Options, typename Options::Enable> {
public:
    virtual bool can_run_with_contribs() { return false; }
};

// ============================================================================
// Option PassTaskExecutor
// ============================================================================
template<typename Options, typename T = typename Options::PassTaskExecutor> class Task_PassTaskExecutor;

template<typename Options>
class Task_PassTaskExecutor<Options, typename Options::Disable> {
public:
    virtual void run() = 0;
};

template<typename Options>
class Task_PassTaskExecutor<Options, typename Options::Enable> {
public:
    virtual void run(TaskExecutor<Options> &) = 0;
};

// ============================================================================
// Option TaskName
// ============================================================================
template<typename Options, typename T = typename Options::TaskName> class Task_TaskName;

template<typename Options>
class Task_TaskName<Options, typename Options::Disable> {};

template<typename Options>
class Task_TaskName<Options, typename Options::Enable> {
public:
    virtual std::string get_name() = 0;
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
  : public Options::ReadyListType::ElementData,
    public detail::Task_PassTaskExecutor<Options>,
    public detail::Task_GlobalId<Options>,
    public detail::Task_TaskName<Options>,
    public detail::Task_Contributions<Options>,
    public detail::Task_Subtasks<Options>
{
    template<typename, typename> friend class Task_PassThreadId;
    template<typename, typename> friend class Task_AccessData;

protected:
    size_t num_access;
    size_t access_idx;
    Access<Options> *access_ptr;

    TaskBaseDefault(size_t num_access_, Access<Options> *access_ptr_)
     : num_access(num_access_), access_idx(0), access_ptr(access_ptr_) {}

public:
    TaskBaseDefault() : num_access(0), access_idx(0), access_ptr(0) {}
    virtual ~TaskBaseDefault() {}

    size_t get_num_access() const { return num_access; }
    Access<Options> *get_access() const { return access_ptr; }
    Access<Options> &get_access(size_t i) const { return access_ptr[i]; }
    bool are_dependencies_solved_or_notify() {
        TaskBase<Options> *this_(static_cast<TaskBase<Options> *>(this));

        for (; access_idx < num_access; ++access_idx) {

            if (!access_ptr[access_idx].get_handle()->is_version_available_or_notify(this_, access_ptr[access_idx].required_version)) {
                ++access_idx; // We consider this dependency fulfilled now, as it will be when we get the callback.
                return false;
            }
        }
        return true;
    }
};

// export Options::TaskBaseType as TaskBase (default: TaskBaseDefault<Options>)
template<typename Options> class TaskBase : public Options::TaskBaseType {};

// ============================================================================
// TaskDefault : adds depend()
// ============================================================================
template<typename Options, typename TaskBaseType, int N = -1>
class TaskAccessMixin : public TaskBaseType {
    Access<Options> access[N];
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename AccessInfo::Type AccessType;
    typedef typename Options::version_t version_t;
    typedef typename Options::lockcount_t lockcount_t;
public:
    TaskAccessMixin() {
        // If this assignment is done through a constructor, we can save the initial assignment,
        // but then any user-overloaded TaskBaseDefault() class must forward both constructors.
        TaskBaseType::access_ptr = &access[0];
    }
    void fulfill(AccessType type, Handle<Options> &handle, version_t version) {
        Access<Options> &a(access[TaskBaseType::num_access]);
        a.handle = &handle;
        a.required_version = version;
        if (AccessUtil<Options>::needs_lock(type))
            a.set_required_quantity(1);
        Options::LogDAG::add_dependency(static_cast<TaskBaseType *>(this), &handle, version, type);
        ++TaskBaseType::num_access;
    }
    void register_access(AccessType type, Handle<Options> &handle) {
        fulfill(type, handle, handle.schedule(type));
    }
    void require(Resource<Options> &resource, lockcount_t quantity = 1) {
        Access<Options> &a(access[TaskBaseType::num_access]);
        a.handle = &resource;
        a.required_version = 0;
        a.set_required_quantity(quantity);
        //Options::LogDAG::add_dependency(static_cast<TaskBaseType *>(this), &handle, version, type);
        ++TaskBaseType::num_access;
    }
};

// Specialization for zero dependencies
template<typename Options, typename TaskBaseType>
class TaskAccessMixin<Options, TaskBaseType, 0> : public TaskBaseType {};

// Specialization for variable number of dependencies
template<typename Options, typename TaskBaseType>
class TaskAccessMixin<Options, TaskBaseType, -1> : public TaskBaseType {
    typedef typename Options::AccessInfoType AccessInfo;
    typedef typename AccessInfo::Type AccessType;
    typedef typename Types<Options>::template vector_t< Access<Options> >::type access_vector_t;
    typedef typename Options::version_t version_t;
    typedef typename Options::lockcount_t lockcount_t;
protected:
    access_vector_t access;
public:
    void fulfill(AccessType type, Handle<Options> &handle, version_t version) {
        access.push_back(Access<Options>(&handle, version));
        Access<Options> &a(access[access.size()-1]);
        if (AccessUtil<Options>::needs_lock(type))
            a.set_required_quantity(1);
        Options::LogDAG::add_dependency(static_cast<TaskBaseType *>(this), &handle, version, type);
        ++TaskBaseType::num_access;
        TaskBase<Options>::access_ptr = &access[0]; // vector may be reallocated at any add
    }

    void register_access(AccessType type, Handle<Options> &handle) {
        fulfill(type, handle, handle.schedule(type));
    }
    void require(Resource<Options> &resource, lockcount_t quantity = 1) {
        access.push_back(Access<Options>(&resource, 0));
        Access<Options> &a(access[access.size()-1]);
        a.set_required_quantity(quantity);
        ++TaskBaseType::num_access;
        TaskBase<Options>::access_ptr = &access[0]; // vector may be reallocated at any add
    }
};

// export "Options::TaskType<>::type" (default: TaskDefault) as type Task
template<typename Options, int N = -1> class Task
 : public Options::template TaskType<N>::type {};

} // namespace sg

#endif // SG_TASK_HPP_INCLUDED
