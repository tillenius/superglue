//
// log which task is executed where, and for what duration
//

#ifndef SG_LOG_HPP_INCLUDED
#define SG_LOG_HPP_INCLUDED

#include "sg/core/spinlock.hpp"
#include "sg/platform/threads.hpp"
#include "sg/platform/gettime.hpp"
#include "sg/platform/tls.hpp"
#include <vector>
#include <fstream>
#include <algorithm>
#include <string>

namespace sg {

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
    static std::string get_name(TaskBase<Options> *task, typename Options::Enable) {
        return task->get_name();
    }

    template<typename Options>
    static std::string get_name(TaskBase<Options> *task, typename Options::Disable) {
        char name[30];
        sprintf(name, "%p", (void *) task);
        return name;
    }

    template<typename Options>
    static std::string get_name(HandleBase<Options> *handle, typename Options::Enable) {
        return handle->get_name();
    }

    template<typename Options>
    static std::string get_name(HandleBase<Options> *handle, typename Options::Disable) {
        char name[30];
        sprintf(name, "%p", (void *) handle);
        return name;
    }

public:

    template<typename Options>
    static std::string get_name(TaskBase<Options> *task) { return get_name(task, typename Options::TaskName()); }

    template<typename Options>
    static std::string get_name(HandleBase<Options> *handle) { return get_name(handle, typename Options::HandleName()); }

};

template<typename Options>
std::ostream& operator<<(std::ostream& os, TaskBase<Options> *task) {
    os << GetName::get_name(task);
    return os;
}
template<typename Options>
std::ostream& operator<<(std::ostream& os, HandleBase<Options> *handle) {
    os << GetName::get_name(handle);
    return os;
}

} // namespace detail

// ===========================================================================
// Log
// ===========================================================================
template<typename Options>
class Log {
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

    static void log(const char *text, Time::TimeUnit start, Time::TimeUnit stop) {
        ThreadData &data(getThreadData());
        data.events.push_back(Event(text, start, stop));
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

    static void register_thread(int id) {
        LogData &data(getLogData());
        ThreadData *td = new ThreadData(id);
        data.tlsdata.set(td);

        SpinLockScoped lock(data.initspinlock);
        data.threaddata.push_back(td);
    }
};

} // namespace sg

#endif // SG_LOG_HPP_INCLUDED
