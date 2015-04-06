#ifndef SG_PLATFORM_HPP_INCLUDED
#define SG_PLATFORM_HPP_INCLUDED

#include <cstdlib>
#include <string>

#ifdef _WIN32
#define SG_INLINE __forceinline
#else
#define SG_INLINE __attribute__((always_inline))
#endif

std::string sg_getenv(const char *env) {
#ifdef _WIN32
    char *buf;
    size_t len;
    errno_t err = _dupenv_s(&buf, &len, env);
    if (err || buf == NULL)
        return "";
    std::string ret(buf);
    free(buf);
    return ret;
#else
    const char *ret = getenv(env);
    return ret ? ret : "";
#endif
}


#endif // SG_PLATFORM_HPP_INCLUDED
