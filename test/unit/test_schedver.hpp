#ifndef SG_TEST_SCHEDVER_HPP_INCLUDED
#define SG_TEST_SCHEDVER_HPP_INCLUDED

#include "sg/option/access_rwc.hpp"
#include <string>

using namespace sg;

class TestSchedulerVer : public TestCase {
    class ReadWriteAddMul {
    public:
        enum Type { read = 0, write, add, mul, num_accesses };
        template<int n> struct AccessType {};
    };

    struct OpDefault : public DefaultOptions<OpDefault> {
        typedef ReadWriteAdd AccessInfoType;
    };

    struct OpMul : public DefaultOptions<OpDefault> {
        typedef ReadWriteAddMul AccessInfoType;
    };

    struct OpRWC : public DefaultOptions<OpDefault> {
        typedef ReadWriteConcurrent AccessInfoType;
    };

    static bool testSchedVer(std::string &name) { name = "testSchedVer";
        SchedulerVersion<OpDefault> s;

        if (s.next_version() != 1) return false;
        if (s.next_version() != 1) return false;

        if (s.schedule(ReadWriteAdd::read) != 0) return false;
        if (s.schedule(ReadWriteAdd::read) != 0) return false;

        if (s.schedule(ReadWriteAdd::add) != 2) return false;
        if (s.schedule(ReadWriteAdd::add) != 2) return false;

        if (s.schedule(ReadWriteAdd::write) != 4) return false;
        if (s.schedule(ReadWriteAdd::write) != 5) return false;

        if (s.next_version() != 7) return false;

        SchedulerVersion<OpMul> s2;

        if (s2.next_version() != 1) return false;
        if (s2.next_version() != 1) return false;

        if (s2.schedule(ReadWriteAddMul::mul) != 0) return false;
        if (s2.schedule(ReadWriteAddMul::mul) != 0) return false;

        if (s2.schedule(ReadWriteAddMul::add) != 2) return false;
        if (s2.schedule(ReadWriteAddMul::add) != 2) return false;

        if (s2.schedule(ReadWriteAddMul::read) != 4) return false;
        if (s2.schedule(ReadWriteAddMul::read) != 4) return false;

        if (s2.schedule(ReadWriteAddMul::write) != 6) return false;
        if (s2.schedule(ReadWriteAddMul::write) != 7) return false;

        if (s2.next_version() != 9) return false;

        return true;
    }

    static bool testAccessUtil(std::string &name) { name = "testAccessUtil";
        if (AccessUtil<OpDefault>::needs_lock(ReadWriteAdd::read)) return false;
        if (!AccessUtil<OpDefault>::needs_lock(ReadWriteAdd::add)) return false;
        if (AccessUtil<OpDefault>::needs_lock(ReadWriteAdd::write)) return false;

        if (AccessUtil<OpMul>::needs_lock(ReadWriteAddMul::read)) return false;
        if (!AccessUtil<OpMul>::needs_lock(ReadWriteAddMul::add)) return false;
        if (!AccessUtil<OpMul>::needs_lock(ReadWriteAddMul::mul)) return false;
        if (AccessUtil<OpMul>::needs_lock(ReadWriteAddMul::write)) return false;

        if (AccessUtil<OpRWC>::needs_lock(ReadWriteConcurrent::read)) return false;
        if (AccessUtil<OpRWC>::needs_lock(ReadWriteConcurrent::write)) return false;
        if (AccessUtil<OpRWC>::needs_lock(ReadWriteConcurrent::concurrent)) return false;
        return true;
    }

public:

    std::string get_name() { return "TestSchedulerVer"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testSchedVer, testAccessUtil
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

template<> struct TestSchedulerVer::ReadWriteAddMul::AccessType<TestSchedulerVer::ReadWriteAddMul::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
};

template<> struct TestSchedulerVer::ReadWriteAddMul::AccessType<TestSchedulerVer::ReadWriteAddMul::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
};

template<> struct TestSchedulerVer::ReadWriteAddMul::AccessType<TestSchedulerVer::ReadWriteAddMul::add> {
    enum { commutative = 1 };
    enum { exclusive = 1 };
};

template<> struct TestSchedulerVer::ReadWriteAddMul::AccessType<TestSchedulerVer::ReadWriteAddMul::mul> {
    enum { commutative = 1 };
    enum { exclusive = 1 };
};

#endif // SG_TEST_SCHEDVER_HPP_INCLUDED
