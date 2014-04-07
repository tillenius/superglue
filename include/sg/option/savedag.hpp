#ifndef SG_SAVEDAG_HPP_INCLUDED
#define SG_SAVEDAG_HPP_INCLUDED

//
// collect DAG log data
//

#include "sg/option/log.hpp"
#include "sg/core/accessutil.hpp"
#include "sg/core/spinlock.hpp"
#include "sg/platform/gettime.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>

namespace sg {

template<typename Options> class Handle;

namespace detail {

struct Node {
    std::string name;
    std::string style;
    int type;
    Time::TimeUnit time_stamp;
    Node(const std::string &name_,
         const std::string &style_,
         int type_)
    : name(name_), style(style_), type(type_) {
        time_stamp = Time::getTime();
    }
};
struct TaskFinish {
    size_t taskid;
    size_t handleid;
    size_t version;
    TaskFinish(size_t taskid_,
               size_t handleid_,
               size_t version_)
    : taskid(taskid_), handleid(handleid_), version(version_)
    {}
};
struct TaskDependency {
    size_t taskid;
    size_t handleid;
    size_t version;
    size_t type;
    TaskDependency(size_t taskid_,
                   size_t handleid_,
                   size_t version_,
                   size_t type_)
    : taskid(taskid_), handleid(handleid_), version(version_), type(type_)
    {}
};

struct LogDagData {
    SpinLock spinlock;

    std::vector<Node> nodes;
    std::map<size_t, size_t> tasknodes;
    std::map< std::pair<size_t, size_t>, size_t> datanodes;
    std::vector<size_t> ranks;

    std::vector<TaskFinish> task_finish;
    std::vector<TaskDependency> task_dependency;

    void clear() {
        nodes.clear();
        tasknodes.clear();
        datanodes.clear();
        ranks.clear();
        task_finish.clear();
        task_dependency.clear();
    }
};

// ===========================================================================
// GetStyle
// ===========================================================================

template<typename T>
class TypeHasGetStyle {
private:
    typedef char yes;
    typedef struct { char dummy[2]; } no;

    template<typename U, std::string (U::*)()> struct TypeCheck {};
    template<typename U> static yes test(TypeCheck<U, &U::get_style>*);
    template<typename U> static no test(...);

public:
    enum { value = (sizeof(test<T>(0)) == sizeof(yes)) };
};

template<typename Options, bool> struct GetStyle {
    static std::string get_style(TaskBase<Options> *) {
        return "";
    }
};
template<typename Options>
struct GetStyle<Options, true> {
    static std::string get_style(TaskBase<Options> *task) {
        return task->get_style();
    }
};

template<typename Options>
std::string get_style(TaskBase<Options> *task) {
    return GetStyle<Options, TypeHasGetStyle< TaskBase<Options> >::value >::get_style(task);
}

} // namespace detail

// ===========================================================================
// Option Logging_DAG
// ===========================================================================

template<typename Options>
class SaveDAG {
    typedef typename Options::version_t version_t;
private:
    static size_t add_node(detail::LogDagData &data, std::string name, std::string style, size_t type) {
        data.nodes.push_back(detail::Node(name, style, type));
        return data.nodes.size()-1;
    }

    static void register_task_node(detail::LogDagData &data, TaskBase<Options> *task) {
        size_t taskId = task->get_global_id();
        if (data.tasknodes.find(taskId) != data.tasknodes.end())
            return;
        std::stringstream ss;
        ss << task;
        size_t new_node = add_node(data, ss.str(), get_style(task), 0);
        data.tasknodes[taskId] = new_node;
    }

    static void register_data_node(detail::LogDagData &data, Handle<Options> *handle, size_t version) {
        size_t handleId = handle->get_global_id();
        std::pair<size_t, size_t> id(handleId, version);
        if (data.datanodes.find(id) != data.datanodes.end())
            return;
        std::stringstream ss;
        ss << "$" << handle << "_{" << version << "}$";
        size_t new_node = add_node(data, ss.str(), "[shape=rectangle,style=filled,fillcolor=gray]", 1);
        data.datanodes[id] = new_node;
    }


public:
    static detail::LogDagData &get_dag_data() {
        static detail::LogDagData data;
        return data;
    }

    static void task_finish(TaskBase<Options> *task, Handle<Options> *handle, size_t newVersion) {
        detail::LogDagData &data(get_dag_data());
        SpinLockScoped lock(data.spinlock);
        // have to register nodes here, as we cannot get name otherwise.
        register_data_node(data, handle, newVersion);
        data.task_finish.push_back(detail::TaskFinish(task->get_global_id(), handle->get_global_id(), newVersion));
    }

    static void add_dependency(TaskBase<Options> *task, Handle<Options> *handle, version_t required_version, int type) {
        detail::LogDagData &data(get_dag_data());
        SpinLockScoped lock(data.spinlock);
        // have to register nodes here, as we cannot get name otherwise.
        register_task_node(data, task);
        register_data_node(data, handle, required_version);
        data.task_dependency.push_back(detail::TaskDependency(task->get_global_id(),
                                                              handle->get_global_id(),
                                                              required_version, type));
    }

    static void new_rank() {
        detail::LogDagData &data(get_dag_data());
        if (data.nodes.empty())
            return;
        if (!data.ranks.empty()) {
            size_t old = data.ranks[data.ranks.size()-1];
            if (data.nodes.size() == old)
                return;
        }
        data.ranks.push_back(data.nodes.size());
    };
};

} // namespace sg
#endif // SG_SAVEDAG_HPP_INCLUDED
