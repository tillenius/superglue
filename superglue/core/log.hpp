#ifndef __LOG_HPP__
#define __LOG_HPP__

#ifdef __linux__
#define ENABLE_PERF
#endif

#include "platform/spinlock.hpp"
#include "platform/threads.hpp"
#include "platform/gettime.hpp"
#ifdef ENABLE_PERF
#include "platform/perfcount.hpp"
#endif
#include "core/accessutil.hpp"
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <stack>
#include <algorithm>
#include <string>
#include <sstream>
#include <signal.h>

template<typename Options> class Log;
template<typename Options> class TaskBase;
template<typename Options> class Handle;
template<typename Options> class TaskExecutor;
template<typename Options> class ThreadManager;
template<typename Options> class TaskQueue;
template<typename Options> class WorkerThread;

namespace detail {

// ===========================================================================
// GetName
// ===========================================================================

class GetName {
private:

    template<typename O>
    static std::string getName(TaskBase<O> *task, typename O::Enable) {
        return task->getName();
    }

    template<typename O>
    static std::string getName(TaskBase<O> *task, typename O::Disable) {
        char name[30];
        sprintf(name, "%p", task);
        return name;
    }

    template<typename O>
    static std::string getName(Handle<O> *handle, typename O::Enable) {
        return handle->getName();
    }

    template<typename O>
    static std::string getName(Handle<O> *handle, typename O::Disable) {
        char name[30];
        sprintf(name, "%p", handle);
        return name;
    }

public:

    template<typename O>
    static std::string getName(TaskBase<O> *task) { return getName(task, typename O::TaskName()); }

    template<typename O>
    static std::string getName(Handle<O> *handle) { return getName(handle, typename O::HandleName()); }

};

template<typename Options>
std::ostream& operator<<(std::ostream& os, TaskBase<Options> *task) {
    os << GetName::getName(task);
    return os;
}
template<typename Options>
std::ostream& operator<<(std::ostream& os, Handle<Options> *handle) {
    os << GetName::getName(handle);
    return os;
}

// ===========================================================================
// Option Logging_DAG
// ===========================================================================
template<typename Options, typename T = typename Options::Logging_DAG> class Log_DAG;

template<typename Options>
class Log_DAG<Options, typename Options::Disable> {
public:
    static void dump_dag(const char *) {}
    static void dump_data_dag(const char *) {}
    static void addDependency(TaskBase<Options> *, Access<Options> *, int) {}
    static void dag_newrank() {};
};

// TODO: Move to central location. Make error message more clear?
template<typename Options, typename T> struct require {
    typedef void CONFIGURATION_ERROR;
};
template<typename Options>
struct require<Options, typename Options::Disable> {};


template<typename Options>
class Log_DAG<Options, typename Options::Enable> {
private:

    typedef typename require<Options, typename Options::TaskId>::CONFIGURATION_ERROR REQUIRES_TASKID;
    typedef typename require<Options, typename Options::HandleId>::CONFIGURATION_ERROR REQUIRES_HANDLEID;

    typedef std::pair<typename Options::version_t, Handle<Options> *> datapair;
    typedef size_t node_t;

    struct Node {
        std::string name;
        std::string style;
        int type;
        time_t time_stamp;
        Node(const std::string &name_,
             const std::string &style_,
             size_t type_)
        :   name(name_), style(style_), type(type_) {
            time_stamp = Time::getTime();
        }
    };

    struct Edge {
        node_t source;
        node_t sink;
        std::string style;
        time_t time_stamp;
        size_t type;
        Edge(node_t source_, node_t sink_, const char *style_, size_t type_)
        : source(source_), sink(sink_), style(style_), type(type_) {
            time_stamp = Time::getTime();
        }
        bool operator<(const Edge &rhs) const {
            if (source < rhs.source)
                return true;
            if (source > rhs.source)
                return false;
            return sink < rhs.sink;
        }
    };

    struct AccessingTasks {
        size_t lasttype;
        std::vector<size_t> last[Options::AccessInfoType::numAccesses];
        AccessingTasks() : lasttype(~0) {}
    };

    struct LogDagData {
        SpinLock spinlock;
        std::vector<Node> nodes;
        std::set<Edge> edges;
        std::map<size_t, node_t> tasknodes;
        std::map<size_t, AccessingTasks> accessinfo;
        std::vector<size_t> ranks;

        void clear() {
            nodes.clear();
            edges.clear();
            tasknodes.clear();
            accessinfo.clear();
        }
    };

    static LogDagData &getDagData() {
        static LogDagData data;
        return data;
    }

    static node_t addNode(LogDagData &data, std::string name, std::string style, size_t type) {
        data.nodes.push_back(Node(name, style, type));
        return data.nodes.size()-1;
    }

    static void registerTaskNode(LogDagData &data, TaskBase<Options> *task) {
        size_t taskId = task->getGlobalId();
        if (data.tasknodes.find(taskId) != data.tasknodes.end())
            return;
        std::stringstream ss;
        ss << task;
        node_t newNode = addNode(data, ss.str(), "", 0);
        data.tasknodes[taskId] = newNode;
    }

    static node_t getTaskNode(LogDagData &data, size_t taskId) {
        return data.tasknodes[taskId];
    }

public:

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

    static void finalize(LogDagData &data, std::vector<size_t> &last, size_t i) {
#ifdef MANY_EDGES // draw dotted edges between all associative tasks in a group
        for (size_t j = 0; j < last.size(); ++j) {
            for (size_t k = j+1; k < last.size(); ++k) {
                data.edges.insert(Edge(getTaskNode(data, last[j], 0),
                                       getTaskNode(data, last[k]),
                                       "[dir=none,style=dashed,penwidth=2]", i));
            }
        }
#else // draw single path to connect all associative tasks in a group
        for (size_t k = 1; k < last.size(); ++k) {
            data.edges.insert(Edge(getTaskNode(data, last[k-1]),
                                   getTaskNode(data, last[k]),
                                   "[dir=none,style=dashed]", i));
        }
#endif
    }

    static void addDependency(TaskBase<Options> *task, Access<Options> *access, size_t type) {
        LogDagData &data(getDagData());
        SpinLockScoped lock(data.spinlock);

        const size_t handleId = access->getHandle()->getGlobalId();
        const size_t taskId = task->getGlobalId();
        AccessingTasks &at(data.accessinfo[handleId]);

        registerTaskNode(data, task);

        if (at.lasttype != type) {
            for (size_t i = 0; i < Options::AccessInfoType::numAccesses; ++i)
                if (i != at.lasttype)
                    at.last[i].clear();
        }

        for (size_t i = 0; i < Options::AccessInfoType::numAccesses; ++i) {

            if (at.lasttype != type && AccessUtil<Options>::needsLock(i))
                finalize(data, at.last[i], i);

            if (AccessUtil<Options>::needsLock(type))
                continue;
            if (i == type && AccessUtil<Options>::concurrent(type))
                continue;

#ifdef MANY_EDGES // draw dependency edges from all tasks in the previous group of associative tasks
                for (size_t j = 0; j < at.last[i].size(); ++j)
                    data.edges.push_back(Edge(getTaskNode(data, at.last[i][j]),
                                              getTaskNode(data, taskId), "", i));
#else // draw a single dependency edge from the "last" task in the previous group of associative tasks
                if (!at.last[i].empty())
                    data.edges.insert(Edge(getTaskNode(data, at.last[i][at.last[i].size()-1]),
                                           getTaskNode(data, taskId), "", i));
#endif
        }

        at.lasttype = type;
        at.last[type].push_back(taskId);
    }


    static void dump_dag(const char *filename) {
        std::ofstream out(filename);
        LogDagData &data(getDagData());
        SpinLockScoped lock(data.spinlock);

        for (typename std::map<size_t, AccessingTasks>::iterator itr = data.accessinfo.begin(); itr != data.accessinfo.end(); ++itr) {
            AccessingTasks &at(itr->second);
            for (size_t k = 0; k < Options::AccessInfoType::numAccesses; ++k)
                if (AccessUtil<Options>::needsLock(k))
                    finalize(data, at.last[k], k);
        }

        out << "digraph {" << std::endl;
        out << "  overlap=false;" << std::endl;

        bool dupename = false;
        {
            std::set<std::string> names;
            for (size_t i=0; i <data.nodes.size(); ++i)
                names.insert(data.nodes[i].name);
            if (names.size() != data.nodes.size())
                dupename = true;
        }

        if (!data.ranks.empty()) {
            data.ranks.push_back(data.nodes.size());
            size_t j = 0;
            for (size_t i = 0; i < data.nodes.size(); ++i) {
                if (data.ranks[j] == i) {
                    if (j != 0)
                        out << " } " << std::endl;
                    out << " { rank = same; " << std::endl;
                    ++j;
                }
                const Node &n(data.nodes[i]);
                if (n.name.empty())
                    continue;
                if (n.type == 1)
                    continue;
                out << "  \"" << n.name << "_" << i << "\" [label=\"\",shape=circle]" << " " << n.style << ";" << std::endl;
//                if (dupename)
//                    out << "  \"" << n.name << "_" << i << "\" [label=\""<<n.name<<"\"]" << " " << n.style << ";" << std::endl;
//                else
//                    out << "  \"" << n.name << "\" [label=\""<<n.name<<"\"]" << " " << n.style << ";" << std::endl;
            }
            out << " } " << std::endl;
        }
        else {
            for (size_t i = 0; i < data.nodes.size(); ++i) {
                const Node &n(data.nodes[i]);
                if (n.name.empty())
                    continue;
                if (n.type == 1)
                    continue;
                if (dupename)
                    out << "  \"" << n.name << "_" << i << "\" [label=\""<<n.name<<"\"]" << " " << n.style << ";" << std::endl;
                else
                    out << "  \"" << n.name << "\" [label=\""<<n.name<<"\"]" << " " << n.style << ";" << std::endl;
            }
        }

        out << std::endl;

        for (typename std::set<Edge>::iterator itr = data.edges.begin(); itr != data.edges.end(); ++itr) {
            const Edge &e(*itr); // data.edges[i]);
            const Node &source(data.nodes[e.source]);
            const Node &sink(data.nodes[e.sink]);
            if (dupename) {
                out << "  \""
                    << source.name << "_" << e.source
                    << "\" -> \""
                    << sink.name << "_" << e.sink
                    << "\" " << e.style << ";" << std::endl;
            }
            else {
                out << "  \""
                    << source.name
                    << "\" -> \""
                    << sink.name
                    << "\" " << e.style << ";" << std::endl;
            }
        }

        out << "}" << std::endl;
        out.close();
        data.clear();
    }
};

// ===========================================================================
// Option Logging_DumpState
// ===========================================================================
template<typename Options, typename T = typename Options::Logging> struct Log_DumpState;

template<typename Options>
struct Log_DumpState<Options, typename Options::Disable> {
    static void dumpState(ThreadManager<Options> &) {}
    static void installSignalHandler(ThreadManager<Options> &) {}
};

template<typename Options>
struct Log_DumpState<Options, typename Options::Enable> {
    static ThreadManager<Options> **getTm() {
        static ThreadManager<Options> *tm;
        return &tm;
    }

    static void signal_handler(int param) {
        Log<Options>::dumpState(**getTm());
        exit(1);
    }

    static void installSignalHandler(ThreadManager<Options> &tm) {
        *(getTm()) = &tm;
        signal(SIGINT, signal_handler);
    }

    static void dumpState(ThreadManager<Options> &tm) {

        typename Log<Options>::LogData &data(Log<Options>::getLogData());
        const size_t num = data.threadmap.size();

        const size_t numWorkers = tm.getNumWorkers();
        std::cerr << "Num workers = " << numWorkers << std::endl;
        const size_t numQueues = tm.getNumQueues();
        std::cerr << "Num queues = " << numQueues << std::endl;

        const size_t N = 5;
        // copy last N events
        std::vector< std::vector< typename Log<Options>::Event> > evs(num);
        for (size_t i = 0; i < num; ++i) {
            SpinLockScoped lock(data.threaddata[i].lock);
            if (data.threaddata[i].events.size() < N)
                evs[i] = data.threaddata[i].events;
            else
                evs[i].insert(evs[i].begin(), data.threaddata[i].events.end()-N, data.threaddata[i].events.end());
        }

        // normalize times
        // keep a pointer per thread.
        // In each iteration:
        //   find minimum time among the events currently pointed to
        //   replace all events sharing this minimum time with a logical time, and increase pointers for these
        std::vector<size_t> ptr(num, 0);
        size_t time = 0;
        for (;;) {
            time_t minTime = 0;
            bool firstTime = true;

            // find minimum time among events currently pointed to
            for (size_t i = 0; i < num; ++i) {
                if (ptr[i] < evs[i].size()) {
                    if (firstTime) {
                        firstTime = false;
                        minTime = evs[i][ptr[i]].time_start;
                    }
                    else
                        minTime = std::min(minTime, evs[i][ptr[i]].time_start);
                }
            }

            // if no time at all was found, we are done
            if (firstTime)
                break;

            // replace all events sharing this minimum time with a logical time, and increase pointers for these
            for (size_t i = 0; i < num; ++i) {
                if (ptr[i] < evs[i].size() && evs[i][ptr[i]].time_start == minTime)
                    evs[i][ptr[i]++].time_start = time;
            }

            // next logical time
            ++time;
        }

        std::cerr<<"Last events:" << std::endl;
        for (size_t i = 0; i < num; ++i) {
            std::cerr<<"Thread " << i << ": ";
            for (size_t j = 0; j < evs[i].size(); ++j) {
                std::cerr << evs[i][j].time_start << "[" << evs[i][j].name << "] ";
            }
            std::cerr << std::endl;
            if (i == 0) {
                TaskQueue<Options> &queue(tm.barrierProtocol.getTaskQueue());

                std::cerr<< "Main queued=: ";
                TaskBase<Options> *list = queue.root;
                while (list->next != 0) {
                    std::cerr << list << " ";
                    list = list->next;
                }

                std::cerr<<std::endl;
            }
            else {
                const size_t threadid = i-1;
                WorkerThread<Options> *worker(tm.getWorker(threadid));
                assert(data.threadmap[worker->getThread()->getThreadId()] == (int) i);
                TaskQueue<Options> &queue(worker->getTaskQueue());


                std::cerr<< "Worker " << i << ": ";
                TaskBase<Options> *list = queue.root;
                while (list->next != 0) {
                    std::cerr << list << " ";
                    list = list->next;
                }

                std::cerr<<std::endl;
            }
        }
    }
};

// ===========================================================================
// Option Logging
// ===========================================================================
template<typename Options, typename T = typename Options::Logging> class Log_Impl { friend class Options::LOGGING_INVALID::error; };

template<typename Options>
class Log_Impl<Options, typename Options::Disable> {
public:
    static void add(const char *) {}
    static void addEvent(const char *, Time::TimeUnit start, Time::TimeUnit stop) {}
    template<typename ParamT>
    static void add(const char *, ParamT) {}

    static void push() {}
    static void pop(const char *) {}
    template<typename ParamT>
    static void pop(const char *, ParamT) {}

    static void dump(const char *, int = 0) {}
    static void stat(Time::TimeUnit &a, Time::TimeUnit &b) { a = b = 0; }

    static void clear() {}

    static void registerThread(int id) {}
    static void init(size_t) {}
};

template<typename Options>
class Log_Impl<Options, typename Options::Enable> {
public:
    typedef Time::TimeUnit time_t;
    struct Event {
        std::string name;
        time_t time_start;
        time_t time_total;
        Event() {}
        Event(const std::string &name_, time_t start, time_t stop)
        : name(name_), time_start(start), time_total(stop-start) {}
        int operator<(const Event &rhs) const {
            return time_start < rhs.time_start;
        }
    };

    struct ThreadData {
        std::vector<Event> events;
        std::stack<Time::TimeUnit> timestack;
        ThreadData() {
            events.reserve(65536);
        }
    };
    struct LogData {
        SpinLock initspinlock;
        std::map<ThreadIDType, int> threadmap;
        ThreadData *threaddata;
        LogData() : threaddata(0) {}
    };

public:
    static LogData &getLogData() {
        // this is very convenient and does not require a variable definition outside of the class,
        // but the C++ standard does not require initialization to be thread safe until C++11:
        // 6.7 [stmt.dcl]
        //   "If control enters the declaration concurrently while the variable is being initialized,
        //    the concurrent execution shall wait for completion of the initialization."
        static LogData data;
        return data;
    }

    static ThreadData &getThreadData() {
        LogData &data(getLogData());
        const int id = data.threadmap[ThreadUtil::getCurrentThreadId()];
        return data.threaddata[id];
    }

    static void add(const Event &event) {
        ThreadData &data(getThreadData());
        data.events.push_back(event);
    }

    static void addEvent(const char *text, Time::TimeUnit start, Time::TimeUnit stop) {
        add(Event(text, start, stop));
    }

    static void add(const char *text) {
        Time::TimeUnit time = Time::getTime();
        add(Event(text, time, time));
    }

    template<typename ParamT>
    static void add(const char *text, ParamT param) {
        Time::TimeUnit time = Time::getTime();
        std::stringstream ss;
        ss << text << "(" << param << ")";
        add(Event(ss.str(), time, time));
    }

    static void push() {
        Time::TimeUnit startTime = Time::getTime();
        typename Log<Options>::ThreadData &data(Log<Options>::getThreadData());
        data.timestack.push(startTime);
    }

    static void pop(const char *text) {
        Time::TimeUnit endTime = Time::getTime();
        typename Log<Options>::ThreadData &data(Log<Options>::getThreadData());
        Time::TimeUnit startTime = data.timestack.top();
        data.timestack.pop();
        Log<Options>::add(typename Log<Options>::Event(text, startTime, endTime));
    }

    template <typename ParamT>
    static void pop(const char *text, ParamT param) {
        Time::TimeUnit endTime = Time::getTime();
        typename Log<Options>::ThreadData &data(Log<Options>::getThreadData());
        Time::TimeUnit startTime = data.timestack.top();
        data.timestack.pop();
        std::stringstream ss;
        ss << text << "(" << param << ")";
        Log<Options>::add(typename Log<Options>::Event(ss.str(), startTime, endTime));
    }

    static void dump(const char *filename, int node_id = 0) {
        std::ofstream out(filename);
        LogData &data(getLogData());

        const size_t num = data.threadmap.size();
        std::vector<std::pair<Event, size_t> > merged;

        for (size_t i = 0; i < num; ++i) {
            for (size_t j = 0; j < data.threaddata[i].events.size(); ++j)
                merged.push_back(std::make_pair(data.threaddata[i].events[j], i));
        }

        std::sort(merged.begin(), merged.end());

        for (size_t i = 0; i < merged.size(); ++i) {
            out << node_id << " "
                << merged[i].second << ": "
                << merged[i].first.time_start-merged[0].first.time_start << " "
                << merged[i].first.time_total << " "
                << merged[i].first.name << std::endl;
        }

        out.close();
    }

    static void stat(unsigned long long &end, unsigned long long &total) {

        LogData &logdata(getLogData());
        end = 0;
        total = 0;
        unsigned long long minimum = 0;
        bool gotMinimum = false;

        const size_t numt = logdata.threadmap.size();
        for (size_t j = 0; j < numt; ++j) {
            ThreadData &data(logdata.threaddata[j]);
            const size_t num = data.events.size();

            if (num == 0)
                continue;

            if (!gotMinimum) {
                minimum = data.events[0].time_start;
                gotMinimum = true;
            }
            for (size_t i = 0; i < num; ++i) {
                if (data.events[i].time_start < minimum)
                    minimum = data.events[i].time_start;
                if (data.events[i].time_start + data.events[i].time_total > end)
                    end = data.events[i].time_start + data.events[i].time_total;
                total += data.events[i].time_total;
            }
        }
        end -= minimum;
    }

    static void clear() {
        LogData &data(getLogData());
        const size_t num = data.threadmap.size();
        for (size_t i = 0; i < num; ++i)
            data.threaddata[i].events.clear();
    }

    static void registerThread(int id) {
        LogData &data(getLogData());
        SpinLockScoped lock(data.initspinlock);
        data.threadmap[ThreadUtil::getCurrentThreadId()] = id;
    }

    static void init(const size_t numCores) {
        LogData &data(getLogData());
        if (data.threaddata != 0)
            delete [] data.threaddata;
        data.threaddata = new ThreadData[numCores];
        data.threadmap.clear();
        registerThread(0);
    }
};

} // namespace detail

// ===========================================================================
// Log
// ===========================================================================
template<typename Options>
class Log
  : public detail::Log_DAG<Options>,
    public detail::Log_Impl<Options>,
    public detail::Log_DumpState<Options>
{};

#endif // __LOG_HPP__
