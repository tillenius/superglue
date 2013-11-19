#ifndef __INSTR_DEBUG_HPP__
#define __INSTR_DEBUG_HPP__

#include <sstream>

template<typename Options>
struct TaskRunDebug {
    void init(TaskExecutor<Options> &te) {}
    static void destroy(TaskExecutor<Options> &) {}
    static void finalize() {}
    static void runTaskBefore(TaskBase<Options> *) {}
    void runTaskAfter(TaskBase<Options> *task) {
        std::stringstream ss;
        ss << "run '" << task->getName() << "' => ";
        for (size_t i = 0; i < task->getNumAccess(); ++i) {
            ss << " handle " 
               << task->getAccess(i).handle->getGlobalId() 
               << " v" << task->getAccess(i).requiredVersion
               << "-> v" << task->getAccess(i).handle->version+1;
        }
        fprintf(stderr, "%s\n", ss.str().c_str());
    }
    static void taskNotRunDeps() {}
    static void taskNotRunLock() {}
};

#endif // __INSTR_DEBUG_HPP__