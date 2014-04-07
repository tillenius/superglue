#ifndef SG_THREADS_HPP_INCLUDED
#define SG_THREADS_HPP_INCLUDED

// ===========================================================================
// Defines Threads interface, and contains platform specific threading code
// ===========================================================================

#include <cassert>

#ifdef __linux__
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#endif
#ifdef __sun
#include <unistd.h>
#include <cstdlib>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/types.h>
#include <sys/cpuvar.h>
#include <pthread.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#ifndef _WIN32
#define PTHREADS
typedef pthread_t ThreadType;
#else
typedef HANDLE ThreadType;
#endif


namespace sg {

namespace detail {

#ifdef _WIN32
DWORD WINAPI spawn_thread(LPVOID arg);
#else
extern "C" void *spawn_thread(void *arg);
#endif

} // namespace detail

// ===========================================================================
// Thread
// ===========================================================================
class Thread {
private:
    Thread(const Thread &);
    const Thread &operator=(const Thread &);

protected:
    ThreadType thread;

public:
    Thread() {}
    virtual ~Thread() {};
    virtual void run() = 0;

    // void start()

    #ifdef _WIN32
    void start() {
        DWORD threadID;
        Thread *this_(static_cast<Thread *>(this));
        this_->thread = CreateThread(NULL, 0, &detail::spawn_thread, this_, 0, &threadID);
    }
    #else
    void start() {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        Thread *this_(static_cast<Thread *>(this));
        assert(pthread_create(&this_->thread, &attr, &detail::spawn_thread, this_) == 0);
        pthread_attr_destroy(&attr);
    }
    #endif

    // void join()

    #ifdef _WIN32
    void join() {
        Thread *this_(static_cast<Thread *>(this));
        WaitForSingleObject(this_->thread, INFINITE);
    }
    #else
    void join() {
        Thread *this_(static_cast<Thread *>(this));
        void *status;
        pthread_join(this_->thread, &status);
    }
    #endif

};

namespace detail {

#ifdef _WIN32
DWORD WINAPI spawn_thread(LPVOID arg) {
    Thread *t = static_cast<Thread *>(arg);
    t->run();
    return 0;
}
#else
extern "C" void *spawn_thread(void *arg) {
    Thread *t = static_cast<Thread *>(arg);
    t->run();
    return 0;
}
#endif

} // namespace detail

} // namespace sg

#endif // SG_THREADS_HPP_INCLUDED
