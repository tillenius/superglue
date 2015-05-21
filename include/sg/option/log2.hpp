//
// log which task is executed where, and for what duration
//

#ifndef SG_LOG2_HPP_INCLUDED
#define SG_LOG2_HPP_INCLUDED

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

// ===========================================================================
// Log
// ===========================================================================
template<typename Options, typename T>
class Log2 {
public:
    struct Event {
        Time::TimeUnit time_start;
        Time::TimeUnit time_total;
        T data;

        Event() {}
        Event(T data_, Time::TimeUnit start, Time::TimeUnit stop)
        : time_start(start), time_total(stop-start), data(data_) {}
        int operator<(const Event &rhs) const {
            return time_start < rhs.time_start;
        }
    };

    struct ThreadData {
        int id;
        std::vector<Event> events;
        ThreadData(int id_) : id(id_) {
            events.reserve(655360);
        }
    };
    struct LogData {
        SpinLock initspinlock;
        std::vector<ThreadData*> threaddata;
    };

public:
    static LogData &getLogData() {
        static LogData logdata;
        return logdata;
    }

    static ThreadData *&getThreadData() {
        static SG_TLS ThreadData *threaddata;
        return threaddata;
    }

    static void log(T data, Time::TimeUnit start, Time::TimeUnit stop) {
        ThreadData &threaddata(*getThreadData());
        threaddata.events.push_back(Event(data, start, stop));
    }

    static void dump(const char *filename, int node_id = 0) {
        std::ofstream out(filename);
        LogData &logdata(getLogData());

        const size_t num = logdata.threaddata.size();
        std::vector<std::pair<Event, int> > merged;

        for (size_t i = 0; i < num; ++i) {
            for (size_t j = 0; j < logdata.threaddata[i]->events.size(); ++j)
                merged.push_back(std::make_pair(logdata.threaddata[i]->events[j], logdata.threaddata[i]->id));
        }

        std::sort(merged.begin(), merged.end());

        for (size_t i = 0; i < merged.size(); ++i) {
            out << node_id << " "
                << merged[i].second << ": "
                << merged[i].first.time_start << " "
                << merged[i].first.time_total << " "
                << merged[i].first.data << std::endl;
        }

        out.close();
    }

    static void clear() {
        LogData &logdata(getLogData());
        const size_t num = logdata.threaddata.size();
        for (size_t i = 0; i < num; ++i)
            logdata.threaddata[i].events.clear();
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
        LogData &logdata(getLogData());
        ThreadData *td = new ThreadData(id);
        getThreadData() = td;

        SpinLockScoped lock(logdata.initspinlock);
        logdata.threaddata.push_back(td);
    }
};

} // namespace sg

#endif // SG_LOG2_HPP_INCLUDED
