//
// collect dag log data (if Options::Logging_DAG enabled)
//

#ifndef __LOG_DAG_HPP__
#define __LOG_DAG_HPP__

#include "core/accessutil.hpp"
#include "platform/gettime.hpp"
#include "platform/spinlock.hpp"
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>

template<typename Options> class Handle;

struct Node {
    std::string name;
    std::string style;
    int type;
    Time::TimeUnit time_stamp;
    Node(const std::string &name_,
         const std::string &style_,
         size_t type_)
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

    std::vector<TaskFinish> taskFinish;
    std::vector<TaskDependency> taskDependency;

    void clear() {
        nodes.clear();
        tasknodes.clear();
        datanodes.clear();
        ranks.clear();
        taskFinish.clear();
        taskDependency.clear();
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
    template<typename U> static yes test(TypeCheck<U, &U::getStyle>*);
    template<typename U> static no test(...);

public:
    enum { value = (sizeof(test<T>(0)) == sizeof(yes)) };
};

template<typename Options, bool> struct GetStyle {
    static std::string getStyle(TaskBase<Options> *) {
        return "";
    }
};
template<typename Options>
struct GetStyle<Options, true> {
    static std::string getStyle(TaskBase<Options> *task) {
        return task->getStyle();
    }
};

template<typename Options>
std::string getStyle(TaskBase<Options> *task) {
    return GetStyle<Options, TypeHasGetStyle< TaskBase<Options> >::value >::getStyle(task);
}


// ===========================================================================
// Option Logging_DAG
// ===========================================================================

template<typename Options, typename T = typename Options::Logging_DAG> class Log_DAG;

template<typename Options>
class Log_DAG<Options, typename Options::Disable> {
    typedef typename Options::version_t version_t;
public:
    static void dag_taskFinish(TaskBase<Options> *, Handle<Options> *, size_t) {}
    static void dag_addDependency(TaskBase<Options> *, Handle<Options> *, version_t required_version, int) {}
    static void dag_newrank() {};
};

template<typename Options>
class Log_DAG<Options, typename Options::Enable> {
    typedef typename Options::version_t version_t;
private:
    static size_t addNode(LogDagData &data, std::string name, std::string style, size_t type) {
        data.nodes.push_back(Node(name, style, type));
        return data.nodes.size()-1;
    }

    static void registerTaskNode(LogDagData &data, TaskBase<Options> *task) {
        size_t taskId = task->getGlobalId();
        if (data.tasknodes.find(taskId) != data.tasknodes.end())
            return;
        std::stringstream ss;
        ss << task;
        size_t newNode = addNode(data, ss.str(), getStyle(task), 0);
        data.tasknodes[taskId] = newNode;
    }

    static void registerDataNode(LogDagData &data, Handle<Options> *handle, size_t version) {
        size_t handleId = handle->getGlobalId();
        std::pair<size_t, size_t> id(handleId, version);
        if (data.datanodes.find(id) != data.datanodes.end())
            return;
        std::stringstream ss;
        ss << "$" << handle << "_{" << version << "}$";
        size_t newNode = addNode(data, ss.str(), "[shape=rectangle,style=filled,fillcolor=gray]", 1);
        data.datanodes[id] = newNode;
    }


public:
    static LogDagData &getDagData() {
        static LogDagData data;
        return data;
    }

    static void dag_taskFinish(TaskBase<Options> *task, Handle<Options> *handle, size_t newVersion) {
        LogDagData &data(getDagData());
        SpinLockScoped lock(data.spinlock);
        // have to register nodes here, as we cannot get name otherwise.
        registerDataNode(data, handle, newVersion);
        data.taskFinish.push_back(TaskFinish(task->getGlobalId(), handle->getGlobalId(), newVersion));
    }

    static void dag_addDependency(TaskBase<Options> *task, Handle<Options> *handle, version_t required_version, int type) {
        LogDagData &data(getDagData());
        SpinLockScoped lock(data.spinlock);
        // have to register nodes here, as we cannot get name otherwise.
        registerTaskNode(data, task);
        registerDataNode(data, handle, required_version);
        data.taskDependency.push_back(TaskDependency(task->getGlobalId(),
                                                     handle->getGlobalId(),
                                                     required_version, type));
    }

    static void dag_newrank() {
        LogDagData &data(getDagData());
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

#endif // __LOG_DAG_HPP__
