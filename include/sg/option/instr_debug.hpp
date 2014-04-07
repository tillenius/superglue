#ifndef SG_INSTR_DEBUG_HPP_INCLUDED
#define SG_INSTR_DEBUG_HPP_INCLUDED

#include <sstream>
#include <cstdio>

namespace sg {

template <typename Options> class TaskBase;

template <typename Options>
struct TaskRunDebug {
    TaskRunDebug(int threadid) {}
    static void run_task_before(TaskBase<Options> *) {}
    static void run_task_after(TaskBase<Options> *task) {
        std::stringstream ss;
        ss << "run '" << task->get_name() << "' => ";
        for (size_t i = 0; i < task->get_num_access(); ++i) {
            ss << " handle " 
               << task->get_access(i).handle->get_global_id() 
               << " v" << task->get_access(i).required_version
               << "-> v" << task->get_access(i).handle->version+1;
        }
        fprintf(stderr, "%s\n", ss.str().c_str());
    }
};

} // namespace sg

#endif // SG_INSTR_DEBUG_HPP_INCLUDED
