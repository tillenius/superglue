#ifndef __HWMODEL_DEFAULT_HPP_
#define __HWMODEL_DEFAULT_HPP_

struct DefaultHardwareModel {
    enum { CACHE_LINE_SIZE = 64 };
    static int cpumap(int id) { return id; }
    static void init() {}
    static int getNumSockets() { return 1; }
    static int getSocket(int id) { return 0; }
};

#endif // __HWMODEL_DEFAULT_HPP_
