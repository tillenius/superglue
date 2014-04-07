#ifndef SG_SAVEDAG_DATA_HPP_INCLUDED
#define SG_SAVEDAG_DATA_HPP_INCLUDED

//
// create a graph with dependencies between tasks and data from dag log data
//

#include "sg/option/savedag.hpp"
#include "sg/option/savedag_common.hpp"

namespace sg {

template<typename Options>
class SaveDAG_data : public detail::SaveDAG_common<Options> {
private:
    static void get_data_merge_map(const detail::LogDagData &data,
                                   const std::map<size_t, std::vector<std::pair<size_t, size_t> > > &task2data,
                                   const std::map< std::pair<size_t, size_t>, std::vector<size_t> > &data2task,
                                   const std::map< std::pair<size_t, size_t>, size_t> &datanodes,
                                   std::map<size_t, size_t> &data_merge_map) {

        for (typename std::map<size_t, std::vector<std::pair<size_t, size_t> > >::const_iterator itr = task2data.begin();
             itr != task2data.end();
             ++itr)
        {
            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handle_id = itr->second[i].first;
                size_t handle_ver = itr->second[i].second;
                size_t org_handle_ver = handle_ver;

                // find last handle version
                for (;;) {
                    std::pair<size_t, size_t> id(handle_id, handle_ver);

                    // stop if a task depend on this data
                    const std::map< std::pair<size_t, size_t>, std::vector<size_t> >::const_iterator iter(data2task.find(id));
                    if (iter != data2task.end() && !iter->second.empty())
                        break;

                    // stop if next handle version doesnt exist
                    if (datanodes.find(std::make_pair(handle_id, handle_ver+1)) == datanodes.end())
                        break;

                    ++handle_ver;
                }

                const size_t org_handle_node_id(detail::SaveDAG_common<Options>::get_data_node(data, handle_id, org_handle_ver));
                const size_t new_handle_node_id(detail::SaveDAG_common<Options>::get_data_node(data, handle_id, handle_ver));

                data_merge_map[org_handle_node_id] = new_handle_node_id;
            }
        }
    }

    static void get_task2data(const detail::LogDagData &data, std::map<size_t, std::vector<std::pair<size_t, size_t> > > &task2data) {
        for (size_t i = 0; i < data.task_finish.size(); ++i) {
            const size_t taskid(data.task_finish[i].taskid);
            const size_t handleid(data.task_finish[i].handleid);
            const size_t version(data.task_finish[i].version);
            task2data[taskid].push_back( std::make_pair(handleid, version) );
        }
    }
    static void get_data2task(const detail::LogDagData &data, std::map< std::pair<size_t, size_t>, std::vector<size_t> > &data2task) {
        for (size_t i = 0; i < data.task_dependency.size(); ++i) {
            const size_t taskid(data.task_dependency[i].taskid);
            const size_t handleid(data.task_dependency[i].handleid);
            const size_t version(data.task_dependency[i].version);
            data2task[std::make_pair(handleid, version)].push_back(taskid);
        }
    }
    static void get_data_written(const detail::LogDagData &data, std::map<size_t, std::set<size_t> > &datawritten) {
        for (size_t i = 0; i < data.task_dependency.size(); ++i) {
            const size_t type(data.task_dependency[i].type);
            if (AccessUtil<Options>::readonly(type))
                continue;
            const size_t handleid(data.task_dependency[i].handleid);
            const size_t taskid(data.task_dependency[i].taskid);
            datawritten[taskid].insert(handleid);
        }
    }

    static void get_data_nodes_to_hide(const detail::LogDagData &data,
                                   std::map<size_t, size_t> &data_merge_map,
                                   const std::map<size_t, std::vector<std::pair<size_t, size_t> > > &task2data,
                                   const std::map< std::pair<size_t, size_t>, std::vector<size_t> > &data2task,
                                   const std::map< std::pair<size_t, size_t>, size_t> &datanodes,
                                   const std::map<size_t, std::set<size_t> > &datawritten,
                                   std::set<size_t> &hiddenNodes) {

        // remove each data node for which
        //     (1) there is no tasks that write to it,
        // and (2) no tasks depending on it
        for (typename std::map<size_t, std::vector<std::pair<size_t, size_t> > >::const_iterator itr = task2data.begin();
             itr != task2data.end();
             ++itr)
        {
            const size_t task_id = itr->first;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handle_id = itr->second[i].first;
                const size_t handle_ver = itr->second[i].second;
                std::pair<size_t, size_t> handlePair(handle_id, handle_ver);
                const size_t org_handle_node_id(detail::SaveDAG_common<Options>::get_data_node(data, handle_id, handle_ver));
                const size_t handle_node_id(data_merge_map[org_handle_node_id]);

                // keep if written to
                std::map<size_t, std::set<size_t> >::const_iterator writtenby(datawritten.find(task_id));
                if (writtenby != datawritten.end()
                    && writtenby->second.find(handle_id) != writtenby->second.end())
                    continue;

                // keep if task depends on it
                std::map< std::pair<size_t, size_t>, std::vector<size_t> >::const_iterator data2taskitr(data2task.find(itr->second[i]));
                if (data2taskitr != data2task.end() && data2taskitr->second.empty())
                    continue;

                hiddenNodes.insert(handle_node_id);
            }
        }
    }

public:
    static void dump(const char *filename) {
        std::ofstream out(filename);
        detail::LogDagData &data(SaveDAG<Options>::get_dag_data());
        SpinLockScoped lock(data.spinlock);

        std::map<size_t, std::vector<std::pair<size_t, size_t> > > task2data;
        get_task2data(data, task2data);

        std::map< std::pair<size_t, size_t>, std::vector<size_t> > data2task;
        get_data2task(data, data2task);

        const std::vector<detail::Node> &nodes( data.nodes );
        const std::map< std::pair<size_t, size_t>, size_t> &datanodes( data.datanodes );

        std::map<size_t, std::set<size_t> > datawritten;
        get_data_written(data, datawritten);

        out << "digraph {" << std::endl;
        out << "  overlap=false;" << std::endl;

        const bool dupename = detail::SaveDAG_common<Options>::find_dupe_names(nodes);
        detail::SaveDAG_common<Options>::dump_task_nodes(nodes, data.ranks, dupename, out);

        std::map<size_t, size_t> data_merge_map;
        get_data_merge_map(data, task2data, data2task, datanodes, data_merge_map);

        std::set<size_t> hidden_nodes;
        get_data_nodes_to_hide(data, data_merge_map, task2data, data2task, datanodes, datawritten, hidden_nodes);
        detail::SaveDAG_common<Options>::dump_data_nodes(nodes, data_merge_map, hidden_nodes, dupename, out);

        for (typename std::map<size_t, std::vector<std::pair<size_t, size_t> > >::iterator itr = task2data.begin();
             itr != task2data.end();
             ++itr)
        {
            const size_t task_id = itr->first;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handle_id = itr->second[i].first;
                const size_t handle_ver = itr->second[i].second;
                const size_t task_node_id(detail::SaveDAG_common<Options>::get_task_node(data, task_id));
                const detail::Node &task_node(nodes[task_node_id]);
                const size_t org_handle_node_id(detail::SaveDAG_common<Options>::get_data_node(data, handle_id, handle_ver));
                const size_t handle_node_id(data_merge_map[org_handle_node_id]);
                if (hidden_nodes.find(handle_node_id) != hidden_nodes.end())
                    continue;
                const detail::Node &handle_node(nodes[handle_node_id]);
                out << "  " << detail::SaveDAG_common<Options>::get_name(dupename, task_node.name, task_node_id)
                    << " -> " << detail::SaveDAG_common<Options>::get_name(dupename, handle_node.name, handle_node_id) << ";" << std::endl;
            }
        }

        for (typename std::map<std::pair<size_t, size_t>, std::vector<size_t> >::iterator itr = data2task.begin();
            itr != data2task.end();
            ++itr) {

            const size_t handle_id = itr->first.first;
            const size_t handle_ver = itr->first.second;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t task_id = itr->second[i];
                const size_t task_node_id(detail::SaveDAG_common<Options>::get_task_node(data, task_id));
                const detail::Node &task_node(nodes[task_node_id]);
                const size_t handle_node_id(detail::SaveDAG_common<Options>::get_data_node(data, handle_id, handle_ver));
                const detail::Node &handle_node(nodes[handle_node_id]);
                out << "  " << detail::SaveDAG_common<Options>::get_name(dupename, handle_node.name, handle_node_id)
                    << " -> " << detail::SaveDAG_common<Options>::get_name(dupename, task_node.name, task_node_id) << ";" << std::endl;
            }
        }

        out << "}" << std::endl;
        out.close();
    }
};

} // namespace sg

#endif // SG_SAVEDAG_DATA_HPP_INCLUDED
