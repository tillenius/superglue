#ifndef __ACCESSTYPES_HPP__
#define __ACCESSTYPES_HPP__

// =====================================================================
// ReadWriteAdd
// =====================================================================
class ReadWriteAdd {
public:
    enum Type { read = 0, add, write, numAccesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWriteAdd::AccessType<ReadWriteAdd::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWriteAdd::AccessType<ReadWriteAdd::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

template<> struct ReadWriteAdd::AccessType<ReadWriteAdd::add> {
    enum { commutative = 1 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

// =====================================================================
// ReadWrite
// =====================================================================
class ReadWrite {
public:
    enum Type { read = 0, write, numAccesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWrite::AccessType<ReadWrite::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWrite::AccessType<ReadWrite::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

// ============================================================================
// ReadWriteConcurrent
// ============================================================================
class ReadWriteConcurrent {
public:
    enum Type { read = 0, write, concurrent, numAccesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::concurrent> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 0 };
};

namespace detail {

// Specialize SchedulerVersionImpl for ReadWriteAdd
// to use 2 counters instead of 3
template<typename, typename> class SchedulerVersionImpl;
template<typename version_t>
class SchedulerVersionImpl<version_t, ReadWriteAdd> {
private:
    version_t requiredVersion[2];

public:
    SchedulerVersionImpl() {
        requiredVersion[0] = requiredVersion[1] = 0;
    }

    version_t nextVersion() {
        return std::max(requiredVersion[0], requiredVersion[1])+1;
    }

    version_t schedule(int type) {
        const version_t nextVer = nextVersion();
        switch (type) {
        case ReadWriteAdd::read:
            requiredVersion[ReadWriteAdd::add] = nextVer;
            return requiredVersion[ReadWriteAdd::read];
        case ReadWriteAdd::add:
            requiredVersion[ReadWriteAdd::read] = nextVer;
            return requiredVersion[ReadWriteAdd::add];
        }
        requiredVersion[0] = requiredVersion[1] = nextVer;
        return nextVer-1;
    }
};

} // namespace detail

#endif // __ACCESSTYPES_HPP__
