#ifndef __TLS_HPP__
#define __TLS_HPP__

#ifdef _WIN32
#error Not implemented
#endif

#include <pthread.h>

template<typename T>
class tls_data {
private:
    pthread_key_t key;

    tls_data(const tls_data &);
    tls_data &operator=(const tls_data &);

public:
    tls_data() {
        pthread_key_create(&key, NULL);
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

#endif // __TLS_HPP__
