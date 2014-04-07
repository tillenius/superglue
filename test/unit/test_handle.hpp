#ifndef SG_TEST_HANDLE_HPP_INCLUDED
#define SG_TEST_HANDLE_HPP_INCLUDED

#include <string>

class TestHandle : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {};

    struct OpName : public DefaultOptions<OpName> {
        typedef Enable HandleName;
    };
    struct OpID: public DefaultOptions<OpID> {
        typedef Enable HandleId;
    };
    struct OpLockable : public DefaultOptions<OpLockable> {
        typedef ReadWriteAdd AccessInfoType;
        typedef Enable Lockable;
    };
    struct OpSubTasks : public DefaultOptions<OpSubTasks> {
        typedef Enable SubTasks;
    };
    struct OpAll : public DefaultOptions<OpAll> {
        typedef ReadWriteAdd AccessInfoType;
        typedef Enable SubTasks;
        typedef Enable HandleName;
        typedef Enable HandleId;
        typedef Enable Lockable;
    };

    static bool testName(std::string &name) { name = "testName";
        Handle<OpName> h1, h2;
        h1.set_name("A");
        h2.set_name("B");
        return h1.get_name() == std::string("A")
            && h2.get_name() == std::string("B");
    }

    static bool testId(std::string &name) { name = "testId";
        Handle<OpID> h1, h2;
        return (h1.get_global_id() != h2.get_global_id());
    }

    static bool testLockable(std::string &name) { name = "testLockable";
        Handle<OpDefault> h0;
        Handle<OpLockable> h1;

        // check that handle cannot be locked twice
        if (!h1.get_lock(1))
            return false;
        if (h1.get_lock(1))
            return false;

        // check that public lock api is available
        (void) &Handle<OpLockable>::get_lock_or_notify;
        (void) &Handle<OpLockable>::release_lock;
        (void) &Handle<OpLockable>::increase_current_version_no_unlock;

        return true;
    }

    static bool testSubTasks(std::string &name) { name = "testSubTasks";
        // just test compilation
        Handle<OpSubTasks> h1;
        return true;
    }

    static bool testCombos(std::string &name) { name = "testCombos";
        Handle<OpAll> h1;
        return true;
    }

public:
    std::string get_name() { return "TestHandle"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testName, testId, testLockable, testSubTasks, testCombos
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_HANDLE_HPP_INCLUDED

