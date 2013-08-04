
struct LocalHardwareModel : public DefaultHardwareModel {
    static void init() {}
    static int cpumap(int id) { return id; }
};


