extern "C" {
#include "csuperglue.h"
}

#include "sg/superglue.hpp"

#ifdef SG_LOGGING
#include "sg/option/instr_trace.hpp"
#endif
#include "sg/platform/gettime.hpp"

#include <cassert>
#include <map>
#include <vector>
#include <stdarg.h>

struct Options : public DefaultOptions<Options> {
    typedef Enable PauseExecution;
#ifdef SG_LOGGING
    typedef Enable TaskName;
    typedef Trace<Options> Instrumentation;
#endif
};

SuperGlue<Options> *superglue;

class CTaskBase : public Task<Options> {
protected:
    sg_task_function function;
    void *args;

public:
    CTaskBase(sg_task_function function_, void *args_)
    : function(function_), args(args_)
    {}
    virtual ~CTaskBase() {}

    void run() {
        function(args);
    }
};

#ifdef SG_LOGGING

class CTask : public CTaskBase {
protected:
    std::string name;
public:
    CTask(sg_task_function function, void *args_, size_t argsize, const char *name_)
    : CTaskBase(function, new char[argsize]), name(name_ == NULL ? "" : name_) {
        memcpy(args, args_, argsize);
    }
    virtual ~CTask() { delete [] (char *) args; }
    std::string get_name() { return name; }
};

class CInplaceTask : public CTaskBase {
protected:
    std::string name;
public:
    CInplaceTask(sg_task_function function, void *args, const char *name_)
    : CTaskBase(function, args), name(name_ == NULL ? "" : name_) {}
    std::string get_name() { return name; }
};

#else // SG_LOGGING

class CTask : public CTaskBase {
public:
    CTask(sg_task_function function, void *args_, size_t argsize, const char *)
    : CTaskBase(function, new char[argsize]) {
        memcpy(args, args_, argsize);
    }
    virtual ~CTask() { delete [] (char *) args; }
};

class CInplaceTask : public CTaskBase {
public:
    CInplaceTask(sg_task_function function, void *args, const char *)
    : CTaskBase(function, args) {}
};

#endif // SG_LOGGING

extern "C" sg_task_t sg_create_task(sg_task_function function, void *args, size_t argsize, const char *name) {
    CTask *task(new CTask(function, args, argsize, name));
    return (sg_task_t) task;
}

extern "C" sg_task_t sg_create_inplace_task(sg_task_function function, void *args, const char *name) {
    CInplaceTask *task(new CInplaceTask(function, args, name));
    return (sg_task_t) task;
}

extern "C" void sg_register_access(sg_task_t task_, enum sg_access_type type, sg_handle_t handle_) {
    CTask *task((CTask *) task_);
    Handle<Options> *handle((Handle<Options> *) handle_);
    task->register_access((ReadWriteAdd::Type) (type-1), *handle);
}

extern "C" void sg_submit_task(sg_task_t task_) {
    CTask *task((CTask *) task_);
    superglue->submit(task);
}

extern "C" void sg_submit(sg_task_function function, void *args, size_t argsize, const char *name, ...) {
    va_list deps;
    CTask *task(new CTask(function, args, argsize, name));

    va_start(deps, name);

    for (;;) {
        int type = va_arg(deps, int);
        if (type == 0)
            break;
        Handle<Options> *handle = va_arg(deps, Handle<Options> *);

        task->register_access((ReadWriteAdd::Type) (type-1), *handle);
    }
    va_end(deps);
    superglue->submit(task);
}

extern "C" void sg_submit_inplace(sg_task_function function, void *args, const char *name, ...) {
    va_list deps;
    CInplaceTask *task(new CInplaceTask(function, args, name));

    va_start(deps, name);

    for (;;) {
        int type = va_arg(deps, int);
        if (type == 0)
            break;
        Handle<Options> *handle = va_arg(deps, Handle<Options> *);

        task->register_access((ReadWriteAdd::Type) (type-1), *handle);
    }
    va_end(deps);
    superglue->submit(task);
}

extern "C" void sg_wait(sg_handle_t handle) {
    superglue->wait(*(Handle<Options> *) handle);
}

extern "C" void sg_barrier() {
    superglue->barrier();
}

extern "C" sg_handle_t *sg_create_handles(int num) {
    sg_handle_t *mem = new sg_handle_t[num];
    for (int i = 0; i < num; ++i)
        mem[i] = (sg_handle_t) new Handle<Options>();
    return mem;
}

extern "C" void sg_destroy_handles(sg_handle_t *handles, int num) {
    for (int i = 0; i < num; ++i)
        delete (Handle<Options> *) handles[i];
    delete [] handles;
}

extern "C" void sg_init() {
    superglue = new SuperGlue<Options>();
    superglue->start_executing();
}

extern "C" void sg_init_paused() {
    superglue = new SuperGlue<Options>();
}

extern "C" void sg_execute() {
    superglue->start_executing();
}

extern "C" void sg_destroy() {
    delete superglue;
}

extern "C" void sg_write_log(const char *filename) {
#ifdef SG_LOGGING
    Options::Instrumentation::dump(filename);
#endif
}

extern "C" unsigned long long sg_get_time() {
    return Time::getTime();
}

extern "C" void sg_log(const char *name, unsigned long long start, unsigned long long stop) {
#ifdef SG_LOGGING
    Log<Options>::log(name, start, stop);
#endif
}
