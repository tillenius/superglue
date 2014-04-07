#ifndef SG_SAVEDAG_TASK_HPP_INCLUDED
#define SG_SAVEDAG_TASK_HPP_INCLUDED

//
// create a graph with dependencies between tasks from dag log data
//

#include "sg/option/savedag_common.hpp"
#include "sg/option/savedag.hpp"

namespace sg {

template<typename Options>
class SaveDAG_task : public detail::SaveDAG_common<Options> {
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
        std::vector<size_t> last[Options::AccessInfoType::num_accesses];
        AccessingTasks() : lasttype(~static_cast<size_t>(0)) {}
    };

    static void finalize(const detail::LogDagData &data,
                         const std::vector<size_t> &last, size_t i,
                         std::set<Edge> &edges) {
#ifdef MANY_EDGES // draw dotted edges between all associative tasks in a group
        for (size_t j = 0; j < last.size(); ++j) {
            for (size_t k = j+1; k < last.size(); ++k) {
                edges.insert(Edge(detail::SaveDAG_common<Options>::get_task_node(data, last[j], 0),
                                  detail::SaveDAG_common<Options>::get_task_node(data, last[k]),
                                  "[dir=none,style=dashed,penwidth=2]", i));
            }
        }
#else // draw single path to connect all associative tasks in a group
        for (size_t k = 1; k < last.size(); ++k) {
            edges.insert(Edge(detail::SaveDAG_common<Options>::get_task_node(data, last[k-1]),
                              detail::SaveDAG_common<Options>::get_task_node(data, last[k]),
                              "[dir=none,style=dashed]", i));
        }
#endif
    }

    static void get_edges(const detail::LogDagData &data, std::set<Edge> &edges) {

        std::map<size_t, AccessingTasks> accessinfo;

        for (size_t i = 0; i < data.task_dependency.size(); ++i) {
            const size_t type(data.task_dependency[i].type);
            const size_t handleid(data.task_dependency[i].handleid);
            const size_t taskid(data.task_dependency[i].taskid);

            AccessingTasks &at(accessinfo[handleid]);

            if (at.lasttype != type) {
                for (size_t j = 0; j < Options::AccessInfoType::num_accesses; ++j)
                    if (j != at.lasttype)
                        at.last[j].clear();
            }

            for (size_t j = 0; j < Options::AccessInfoType::num_accesses; ++j) {

                if (at.lasttype != type && AccessUtil<Options>::needs_lock(j))
                    finalize(data, at.last[j], j, edges);

                if (AccessUtil<Options>::needs_lock(type))
                    continue;
                if (j == type && AccessUtil<Options>::concurrent(type))
                    continue;

#ifdef MANY_EDGES // draw dependency edges from all tasks in the previous group of associative tasks
                for (size_t k = 0; k < at.last[j].size(); ++k)
                    edges.insert(Edge(detail::SaveDAG_common<Options>::get_task_node(data, at.last[j][k]),
                                      detail::SaveDAG_common<Options>::get_task_node(data, taskId), "", j));
#else // draw a single dependency edge from the "last" task in the previous group of associative tasks
                if (!at.last[j].empty())
                    edges.insert(Edge(detail::SaveDAG_common<Options>::get_task_node(data, at.last[j][at.last[j].size()-1]),
                                      detail::SaveDAG_common<Options>::get_task_node(data, taskid), "", j));
#endif
            }

            at.lasttype = type;
            at.last[type].push_back(taskid);
        }

        for (typename std::map<size_t, AccessingTasks>::const_iterator itr = accessinfo.begin(); itr != accessinfo.end(); ++itr) {
            const AccessingTasks &at(itr->second);
            for (size_t k = 0; k < Options::AccessInfoType::num_accesses; ++k)
                if (AccessUtil<Options>::needs_lock(k))
                    finalize(data, at.last[k], k, edges);
        }
    }

public:
    static void dump(const char *filename) {
        std::ofstream out(filename);
        detail::LogDagData &data(SaveDAG<Options>::get_dag_data());
        SpinLockScoped lock(data.spinlock);

        std::set<Edge> edges;
        get_edges(data, edges);

        const bool dupename = detail::SaveDAG_common<Options>::find_dupe_names( data.nodes );

        out << "digraph {" << std::endl;
        out << "  overlap=false;" << std::endl;

        detail::SaveDAG_common<Options>::dump_task_nodes(data.nodes, data.ranks, dupename, out);

        for (typename std::set<Edge>::iterator itr = edges.begin(); itr != edges.end(); ++itr) {
            const Edge &e(*itr);
            const detail::Node &source(data.nodes[e.source]);
            const detail::Node &sink(data.nodes[e.sink]);
            out << "  " << detail::SaveDAG_common<Options>::get_name(dupename, source.name, e.source) << " -> "
                << detail::SaveDAG_common<Options>::get_name(dupename, sink.name, e.sink) << " " << e.style << ";" << std::endl;
        }

        out << "}" << std::endl;
        out.close();
    }
};

} // namespace sg

#endif // SG_SAVEDAG_TASK_HPP_INCLUDED
