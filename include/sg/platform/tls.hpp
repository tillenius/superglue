#ifndef SG_TLS_HPP_INCLUDED
#define SG_TLS_HPP_INCLUDED

#if defined(_POSIX_THREADS)
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace sg {

#if defined(_POSIX_THREADS)

template<typename T>
class tls_data {
private:
    pthread_key_t key;

    tls_data(const tls_data &);
    tls_data &operator=(const tls_data &);

public:
    tls_data() {
        assert( pthread_key_create(&key, NULL) == 0);
    }
    ~tls_data() {
        pthread_key_delete(key);
    }
    T *get() {
        return (T *) pthread_getspecific(key);
    }
    void set(const T *value) {
        pthread_setspecific(key, (void *) value);
    }
};

#elif defined(_WIN32)

#error Not tested

template<typename T>
class tls_data {
private:
    DWORD key;

    tls_data(const tls_data &);
    tls_data &operator=(const tls_data &);

public:
    tls_data() {
        key = TlsAlloc();
    }
    ~tls_data() {
        pthread_key_delete(key);
    }
    T *get() {
        return (T *) TlsGetValue(key);;
    }
    void set(const T *value) {
        TlsSetValue(key, (void *) value);
    }
};

#else

#error Not Tested

template<typename T>
class tls_data {
private:
    __thread T *value;

    tls_data(const tls_data &);
    tls_data &operator=(const tls_data &);

public:
    T *get() { return value; }
    void set(const T *value_) { value = value_; }
};

#endif

} // namespace sg

#endif // SG_TLS_HPP_INCLUDED
