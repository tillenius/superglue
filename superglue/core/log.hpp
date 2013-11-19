//
// log which task is executed where, and for what duration
//

#ifndef __LOG_HPP__
#define __LOG_HPP__

#include "platform/spinlock.hpp"
#include "platform/threads.hpp"
#include "platform/gettime.hpp"
#include "platform/tls.hpp"
#include <vector>
#include <fstream>
#include <map>
#include <stack>
#include <algorithm>
#include <string>
#include <sstream>

template<typename Options> class Log;
template<typename Options> class TaskBase;
template<typename Options> class HandleBase;

namespace detail {

// ===========================================================================
// GetName
// ===========================================================================

class GetName {
private:

    template<typename Options>
    static std::string getName(TaskBase<Options> *task, typename Options::Enable) {
        return task->getName();
    }

    template<typename Options>
    static std::string getName(TaskBase<Options> *task, typename Options::Disable) {
        char name[30];
        sprintf(name, "%p", (void *) task);
        return name;
    }

    template<typename Options>
    static std::string getName(HandleBase<Options> *handle, typename Options::Enable) {
        return handle->getName();
    }

    template<typename Options>
    static std::string getName(HandleBase<Options> *handle, typename Options::Disable) {
        char name[30];
        sprintf(name, "%p", (void *) handle);
        return name;
    }

public:

    template<typename Options>
    static std::string getName(TaskBase<Options> *task) { return getName(task, typename Options::TaskName()); }

    template<typename Options>
    static std::string getName(HandleBase<Options> *handle) { return getName(handle, typename Options::HandleName()); }

};

template<typename Options>
std::ostream& operator<<(std::ostream& os, TaskBase<Options> *task) {
    os << GetName::getName(task);
    return os;
}
template<typename Options>
std::ostream& operator<<(std::ostream& os, HandleBase<Options> *handle) {
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
    static void init() {}
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
        int id;
        std::vector<Event> events;
        std::stack<Time::TimeUnit> timestack;
        ThreadData(int id_) : id(id_) {
            events.reserve(65536);
        }
    };
    struct LogData {
        tls_data<ThreadData> tlsdata;
        SpinLock initspinlock;
        std::vector<ThreadData*> threaddata;
    };

public:
    static LogData &getLogData() {
        static LogData data;
        return data;
    }

    static ThreadData &getThreadData() {
        return *getLogData().tlsdata.get();
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

        const size_t num = data.threaddata.size();
        std::vector<std::pair<Event, int> > merged;

        for (size_t i = 0; i < num; ++i) {
            for (size_t j = 0; j < data.threaddata[i]->events.size(); ++j)
                merged.push_back(std::make_pair(data.threaddata[i]->events[j], data.threaddata[i]->id));
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

        const size_t numt = logdata.threaddata.size();
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
        const size_t num = data.threaddata.size();
        for (size_t i = 0; i < num; ++i)
            data.threaddata[i].events.clear();
    }

    static void init() {
        // This method only needs to be called once,
        // but it is allowed to call is several times,
        // as long as its from the same thread.

        // The reason to allow this to be called several
        // times is to be able to initialize and use
        // the logging machinery before starting up
        // SuperGlue, but still call here in the SuperGlue
        // startup.

		// However, no initialization is currently needed.
    }

    static void registerThread(int id) {
        LogData &data(getLogData());
        ThreadData *td = new ThreadData(id);
        data.tlsdata.set(td);

        SpinLockScoped lock(data.initspinlock);
        data.threaddata.push_back(td);
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
