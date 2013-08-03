//
// create a graph with dependencies between tasks from dag log data
//

#ifndef __LOG_DAG_TASK_HPP__
#define __LOG_DAG_TASK_HPP__

#include "log_dag_common.hpp"
#include "core/log_dag.hpp"

template<typename Options>
class Log_DAG_task : public Log_DAG_common<Options> {
private:
    struct Edge {
        size_t source;
        size_t sink;
        std::string style;
        Time::TimeUnit time_stamp;
        size_t type;
        Edge(size_t source_, size_t sink_, const char *style_, size_t type_)
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
        AccessingTasks() : lasttype(~static_cast<size_t>(0)) {}
    };

    static void finalize(const LogDagData &data,
                         const std::vector<size_t> &last, size_t i,
                         std::set<Edge> &edges) {
#ifdef MANY_EDGES // draw dotted edges between all associative tasks in a group
        for (size_t j = 0; j < last.size(); ++j) {
            for (size_t k = j+1; k < last.size(); ++k) {
                edges.insert(Edge(Log_DAG_common<Options>::getTaskNode(data, last[j], 0),
                                  Log_DAG_common<Options>::getTaskNode(data, last[k]),
                                  "[dir=none,style=dashed,penwidth=2]", i));
            }
        }
#else // draw single path to connect all associative tasks in a group
        for (size_t k = 1; k < last.size(); ++k) {
            edges.insert(Edge(Log_DAG_common<Options>::getTaskNode(data, last[k-1]),
                              Log_DAG_common<Options>::getTaskNode(data, last[k]),
                              "[dir=none,style=dashed]", i));
        }
#endif
    }

    static void getEdges(const LogDagData &data, std::set<Edge> &edges) {

        std::map<size_t, AccessingTasks> accessinfo;

        for (size_t i = 0; i < data.taskDependency.size(); ++i) {
            const size_t type(data.taskDependency[i].type);
            const size_t handleid(data.taskDependency[i].handleid);
            const size_t taskid(data.taskDependency[i].taskid);

            AccessingTasks &at(accessinfo[handleid]);

            if (at.lasttype != type) {
                for (size_t j = 0; j < Options::AccessInfoType::numAccesses; ++j)
                    if (j != at.lasttype)
                        at.last[j].clear();
            }

            for (size_t j = 0; j < Options::AccessInfoType::numAccesses; ++j) {

                if (at.lasttype != type && AccessUtil<Options>::needsLock(j))
                    finalize(data, at.last[j], j, edges);

                if (AccessUtil<Options>::needsLock(type))
                    continue;
                if (j == type && AccessUtil<Options>::concurrent(type))
                    continue;

#ifdef MANY_EDGES // draw dependency edges from all tasks in the previous group of associative tasks
                for (size_t k = 0; k < at.last[j].size(); ++k)
                    edges.insert(Edge(Log_DAG_common<Options>::getTaskNode(data, at.last[j][k]),
                                      Log_DAG_common<Options>::getTaskNode(data, taskId), "", j));
#else // draw a single dependency edge from the "last" task in the previous group of associative tasks
                if (!at.last[j].empty())
                    edges.insert(Edge(Log_DAG_common<Options>::getTaskNode(data, at.last[j][at.last[j].size()-1]),
                                      Log_DAG_common<Options>::getTaskNode(data, taskid), "", j));
#endif
            }

            at.lasttype = type;
            at.last[type].push_back(taskid);
        }

        for (typename std::map<size_t, AccessingTasks>::const_iterator itr = accessinfo.begin(); itr != accessinfo.end(); ++itr) {
            const AccessingTasks &at(itr->second);
            for (size_t k = 0; k < Options::AccessInfoType::numAccesses; ++k)
                if (AccessUtil<Options>::needsLock(k))
                    finalize(data, at.last[k], k, edges);
        }
    }

public:
    static void dump(const char *filename) {
        std::ofstream out(filename);
        LogDagData &data(Log_DAG<Options>::getDagData());
        SpinLockScoped lock(data.spinlock);

        std::set<Edge> edges;
        getEdges(data, edges);

        const bool dupename = Log_DAG_common<Options>::findDupeNames( data.nodes );

        out << "digraph {" << std::endl;
        out << "  overlap=false;" << std::endl;

        Log_DAG_common<Options>::dumpTaskNodes(data.nodes, data.ranks, dupename, out);

        for (typename std::set<Edge>::iterator itr = edges.begin(); itr != edges.end(); ++itr) {
            const Edge &e(*itr);
            const Node &source(data.nodes[e.source]);
            const Node &sink(data.nodes[e.sink]);
            out << "  " << Log_DAG_common<Options>::getName(dupename, source.name, e.source) << " -> "
                << Log_DAG_common<Options>::getName(dupename, sink.name, e.sink) << " " << e.style << ";" << std::endl;
        }

        out << "}" << std::endl;
        out.close();
    }
};

#endif // __LOG_DAG_TASK_HPP__
