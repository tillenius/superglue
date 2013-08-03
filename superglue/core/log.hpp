//
// log which task is executed where, and for what duration
//

#ifndef __LOG_HPP__
#define __LOG_HPP__

#include "platform/spinlock.hpp"
#include "platform/threads.hpp"
#include "platform/gettime.hpp"
#include <vector>
#include <fstream>
#include <map>
#include <stack>
#include <algorithm>
#include <string>
#include <sstream>

template<typename Options> class Log;
template<typename Options> class TaskBase;
template<typename Options> class Handle;

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
        sprintf(name, "%p", (void *) task);
        return name;
    }

    template<typename O>
    static std::string getName(Handle<O> *handle, typename O::Enable) {
        return handle->getName();
    }

    template<typename O>
    static std::string getName(Handle<O> *handle, typename O::Disable) {
        char name[30];
        sprintf(name, "%p", (void *) handle);
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
    struct Event {
        std::string name;
        Time::TimeUnit time_start;
        Time::TimeUnit time_total;
        Event() {}
        Event(const std::string &name_, Time::TimeUnit start, Time::TimeUnit stop)
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
                << merged[i].first.time_start << " "
                << merged[i].first.time_total << " "
                << merged[i].first.name << std::endl;
        }

        out.close();
    }

    static void stat(Time::TimeUnit &end, Time::TimeUnit &total) {

        LogData &logdata(getLogData());
        end = 0;
        total = 0;
        Time::TimeUnit minimum = 0;
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
  : public detail::Log_Impl<Options>
{};

#endif // __LOG_HPP__
