//
// contains common utilities for classes that generates graphs from dag log data
//

#ifndef __LOG_DAG_COMMON_HPP__
#define __LOG_DAG_COMMON_HPP__

template<typename Options>
class Log_DAG_common {
public:
    //typedef typename require<Options, typename Options::TaskId>::CONFIGURATION_ERROR REQUIRES_TASKID;
    //typedef typename require<Options, typename Options::HandleId>::CONFIGURATION_ERROR REQUIRES_HANDLEID;

    static size_t getDataNode(const LogDagData &data,
                              size_t handleId, size_t version) {
        // data node must exist in datanodes
        return data.datanodes.find(std::make_pair(handleId, version))->second;
    }

    static size_t getTaskNode(const LogDagData &data, size_t taskId) {
        // taskId must be in tasknodes.
        return data.tasknodes.find(taskId)->second;
    }

    static std::string getName(bool dupename, std::string name, size_t index) {
        std::stringstream ss;
        ss << "\"" << name;
        if (dupename)
            ss << "_" << index;
        ss << "\"";
        return ss.str();
    }

    static bool findDupeNames(const std::vector<Node> &nodes) {
        std::set<std::string> names;
        for (size_t i = 0; i < nodes.size(); ++i)
            names.insert(nodes[i].name);
        if (names.size() != nodes.size())
            return true;
        return false;
    }

    static void dumpTaskNodes(const std::vector<Node> &nodes,
                              const std::vector<size_t> &ranks,
                              bool dupename,
                              std::ofstream &out) {
        if (ranks.empty())
            out << "{ " << std::endl;
        size_t j = 0;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (j != ranks.size() && ranks[j] == i) {
                if (j != 0)
                    out << " } " << std::endl;
                out << " { rank = same; " << std::endl;
                ++j;
            }
            const Node &n(nodes[i]);
            if (n.name.empty())
                continue;
            if (n.type == 1)
                continue;
            out << "  " << getName(dupename, n.name, i)
                << " [label=\""<<n.name<<"\"]"
                << " " << n.style
                << ";" << std::endl;
        }
        out << " } " << std::endl;
        out << std::endl;
    }

    static void dumpDataNodes(const std::vector<Node> &nodes,
                              std::map<size_t, size_t> &dataMergeMap,
                              std::set<size_t> &hiddenNodes,
                              bool dupename, std::ofstream &out) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            const Node &n(nodes[i]);
            if (n.name.empty())
                continue;
            if (hiddenNodes.find(i) != hiddenNodes.end())
                continue;
            if (n.type != 1)
                continue;
            if (dataMergeMap.find(i) != dataMergeMap.end()
                && dataMergeMap[i] != i)
                continue;

            out << "  " << getName(dupename, n.name, i)
                << " [label=\""<<n.name<<"\"]"
                << " " << n.style
                << ";" << std::endl;
        }
        out << std::endl;
    }
};

#endif // __LOG_DAG_COMMON_HPP__
