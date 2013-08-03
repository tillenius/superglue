//
// create a graph with dependencies between tasks and data from dag log data
//

#ifndef __LOG_DAG_DATA_HPP__
#define __LOG_DAG_DATA_HPP__

#include "log_dag_common.hpp"
#include "core/log_dag.hpp"

template<typename Options>
class Log_DAG_data : public Log_DAG_common<Options> {
private:
    static void getDataMergeMap(const LogDagData &data,
                                const std::map<size_t, std::vector<std::pair<size_t, size_t> > > &task2data,
                                const std::map< std::pair<size_t, size_t>, std::vector<size_t> > &data2task,
                                const std::map< std::pair<size_t, size_t>, size_t> &datanodes,
                                std::map<size_t, size_t> &dataMergeMap) {

        for (typename std::map<size_t, std::vector<std::pair<size_t, size_t> > >::const_iterator itr = task2data.begin();
             itr != task2data.end();
             ++itr)
        {
            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handleId = itr->second[i].first;
                size_t handleVer = itr->second[i].second;
                size_t orgHandleVer = handleVer;

                // find last handle version
                for (;;) {
                    std::pair<size_t, size_t> id(handleId, handleVer);

                    // stop if a task depend on this data
                    const std::map< std::pair<size_t, size_t>, std::vector<size_t> >::const_iterator iter(data2task.find(id));
                    if (iter != data2task.end() && !iter->second.empty())
                        break;

                    // stop if next handle version doesnt exist
                    if (datanodes.find(std::make_pair(handleId, handleVer+1)) == datanodes.end())
                        break;

                    ++handleVer;
                }

                const size_t orgHandleNodeId(Log_DAG_common<Options>::getDataNode(data, handleId, orgHandleVer));
                const size_t newHandleNodeId(Log_DAG_common<Options>::getDataNode(data, handleId, handleVer));

                dataMergeMap[orgHandleNodeId] = newHandleNodeId;
            }
        }
    }

    static void getTask2Data(const LogDagData &data, std::map<size_t, std::vector<std::pair<size_t, size_t> > > &task2data) {
        for (size_t i = 0; i < data.taskFinish.size(); ++i) {
            const size_t taskid(data.taskFinish[i].taskid);
            const size_t handleid(data.taskFinish[i].handleid);
            const size_t version(data.taskFinish[i].version);
            task2data[taskid].push_back( std::make_pair(handleid, version) );
        }
    }
    static void getData2Task(const LogDagData &data, std::map< std::pair<size_t, size_t>, std::vector<size_t> > &data2task) {
        for (size_t i = 0; i < data.taskDependency.size(); ++i) {
            const size_t taskid(data.taskDependency[i].taskid);
            const size_t handleid(data.taskDependency[i].handleid);
            const size_t version(data.taskDependency[i].version);
            data2task[std::make_pair(handleid, version)].push_back(taskid);
        }
    }
    static void getDataWritten(const LogDagData &data, std::map<size_t, std::set<size_t> > &datawritten) {
        for (size_t i = 0; i < data.taskDependency.size(); ++i) {
            const size_t type(data.taskDependency[i].type);
            if (AccessUtil<Options>::readonly(type))
                continue;
            const size_t handleid(data.taskDependency[i].handleid);
            const size_t taskid(data.taskDependency[i].taskid);
            datawritten[taskid].insert(handleid);
        }
    }

    static void getDataNodesToHide(const LogDagData &data,
                                   std::map<size_t, size_t> &dataMergeMap,
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
            const size_t taskId = itr->first;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handleId = itr->second[i].first;
                const size_t handleVer = itr->second[i].second;
                std::pair<size_t, size_t> handlePair(handleId, handleVer);
                const size_t orgHandleNodeId(Log_DAG_common<Options>::getDataNode(data, handleId, handleVer));
                const size_t handleNodeId(dataMergeMap[orgHandleNodeId]);

                // keep if written to
                std::map<size_t, std::set<size_t> >::const_iterator writtenby(datawritten.find(taskId));
                if (writtenby != datawritten.end()
                    && writtenby->second.find(handleId) != writtenby->second.end())
                    continue;

                // keep if task depends on it
                std::map< std::pair<size_t, size_t>, std::vector<size_t> >::const_iterator data2taskitr(data2task.find(itr->second[i]));
                if (data2taskitr != data2task.end() && data2taskitr->second.empty())
                    continue;

                hiddenNodes.insert(handleNodeId);
            }
        }
    }

public:
    static void dump(const char *filename) {
        std::ofstream out(filename);
        LogDagData &data(Log_DAG<Options>::getDagData());
        SpinLockScoped lock(data.spinlock);

        std::map<size_t, std::vector<std::pair<size_t, size_t> > > task2data;
        getTask2Data(data, task2data);

        std::map< std::pair<size_t, size_t>, std::vector<size_t> > data2task;
        getData2Task(data, data2task);

        const std::vector<Node> &nodes( data.nodes );
        const std::map< std::pair<size_t, size_t>, size_t> &datanodes( data.datanodes );

        std::map<size_t, std::set<size_t> > datawritten;
        getDataWritten(data, datawritten);

        out << "digraph {" << std::endl;
        out << "  overlap=false;" << std::endl;

        const bool dupename = Log_DAG_common<Options>::findDupeNames(nodes);
        Log_DAG_common<Options>::dumpTaskNodes(nodes, data.ranks, dupename, out);

        std::map<size_t, size_t> dataMergeMap;
        getDataMergeMap(data, task2data, data2task, datanodes, dataMergeMap);

        std::set<size_t> hiddenNodes;
        getDataNodesToHide(data, dataMergeMap, task2data, data2task, datanodes, datawritten, hiddenNodes);
        Log_DAG_common<Options>::dumpDataNodes(nodes, dataMergeMap, hiddenNodes, dupename, out);

        for (typename std::map<size_t, std::vector<std::pair<size_t, size_t> > >::iterator itr = task2data.begin();
             itr != task2data.end();
             ++itr)
        {
            const size_t taskId = itr->first;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t handleId = itr->second[i].first;
                const size_t handleVer = itr->second[i].second;
                const size_t taskNodeId(Log_DAG_common<Options>::getTaskNode(data, taskId));
                const Node &taskNode(nodes[taskNodeId]);
                const size_t orgHandleNodeId(Log_DAG_common<Options>::getDataNode(data, handleId, handleVer));
                const size_t handleNodeId(dataMergeMap[orgHandleNodeId]);
                if (hiddenNodes.find(handleNodeId) != hiddenNodes.end())
                    continue;
                const Node &handleNode(nodes[handleNodeId]);
                out << "  " << Log_DAG_common<Options>::getName(dupename, taskNode.name, taskNodeId)
                    << " -> " << Log_DAG_common<Options>::getName(dupename, handleNode.name, handleNodeId) << ";" << std::endl;
            }
        }

        for (typename std::map<std::pair<size_t, size_t>, std::vector<size_t> >::iterator itr = data2task.begin();
            itr != data2task.end();
            ++itr) {

            const size_t handleId = itr->first.first;
            const size_t handleVer = itr->first.second;

            for (size_t i = 0; i < itr->second.size(); ++i) {
                const size_t taskId = itr->second[i];
                const size_t taskNodeId(Log_DAG_common<Options>::getTaskNode(data, taskId));
                const Node &taskNode(nodes[taskNodeId]);
                const size_t handleNodeId(Log_DAG_common<Options>::getDataNode(data, handleId, handleVer));
                const Node &handleNode(nodes[handleNodeId]);
                out << "  " << Log_DAG_common<Options>::getName(dupename, handleNode.name, handleNodeId)
                    << " -> " << Log_DAG_common<Options>::getName(dupename, taskNode.name, taskNodeId) << ";" << std::endl;
            }
        }

        out << "}" << std::endl;
        out.close();
    }
};

#endif // __LOG_DAG_DATA_HPP__
