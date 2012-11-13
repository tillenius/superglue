#ifndef __HWMODEL_DEFAULT_HPP_
#define __HWMODEL_DEFAULT_HPP_

struct DefaultHardwareModel {
    enum { CACHE_LINE_SIZE = 64 };
    static int cpumap(int id) { return id; }
};

#endif // __HWMODEL_DEFAULT_HPP_
