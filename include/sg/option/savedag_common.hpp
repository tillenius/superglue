#ifndef SG_SAVEDAG_COMMON_HPP_INCLUDED
#define SG_SAVEDAG_COMMON_HPP_INCLUDED

//
// contains common utilities for classes that generates graphs from dag log data
//

#include "sg/option/savedag.hpp"

namespace sg {

namespace detail {

template<typename Options>
class SaveDAG_common {
public:
    static size_t get_data_node(const LogDagData &data,
                                size_t handle_id, size_t version) {
        // data node must exist in datanodes
        return data.datanodes.find(std::make_pair(handle_id, version))->second;
    }

    static size_t get_task_node(const LogDagData &data, size_t task_id) {
        // task_id must be in tasknodes.
        return data.tasknodes.find(task_id)->second;
    }

    static std::string get_name(bool dupename, std::string name, size_t index) {
        std::stringstream ss;
        ss << "\"" << name;
        if (dupename)
            ss << "_" << index;
        ss << "\"";
        return ss.str();
    }

    static bool find_dupe_names(const std::vector<Node> &nodes) {
        std::set<std::string> names;
        for (size_t i = 0; i < nodes.size(); ++i)
            names.insert(nodes[i].name);
        if (names.size() != nodes.size())
            return true;
        return false;
    }

    static void dump_task_nodes(const std::vector<Node> &nodes,
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
            out << "  " << get_name(dupename, n.name, i)
                << " [label=\""<<n.name<<"\"]"
                << " " << n.style
                << ";" << std::endl;
        }
        out << " } " << std::endl;
        out << std::endl;
    }

    static void dump_data_nodes(const std::vector<Node> &nodes,
                                std::map<size_t, size_t> &data_merge_map,
                                 std::set<size_t> &hidden_nodes,
                                bool dupename, std::ofstream &out) {
        for (size_t i = 0; i < nodes.size(); ++i) {
            const Node &n(nodes[i]);
            if (n.name.empty())
                continue;
            if (hidden_nodes.find(i) != hidden_nodes.end())
                continue;
            if (n.type != 1)
                continue;
            if (data_merge_map.find(i) != data_merge_map.end()
                && data_merge_map[i] != i)
                continue;

            out << "  " << get_name(dupename, n.name, i)
                << " [label=\""<<n.name<<"\"]"
                << " " << n.style
                << ";" << std::endl;
        }
        out << std::endl;
    }
};

} // namespace detail

} // namespace sg

#endif // SG_SAVEDAG_COMMON_HPP_INCLUDED
