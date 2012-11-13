#ifndef __TEST_HANDLE_HPP_
#define __TEST_HANDLE_HPP_

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
        h1.setName("A");
        h2.setName("B");
        return h1.getName() == std::string("A")
            && h2.getName() == std::string("B");
    }

    static bool testId(std::string &name) { name = "testId";
        Handle<OpID> h1, h2;
        return (h1.getGlobalId() != h2.getGlobalId());
    }

    static bool testLockable(std::string &name) { name = "testLockable";
        Handle<OpDefault> h0;
        Handle<OpLockable> h1;

        // check that handle cannot be locked twice
        if (!h1.getLock())
            return false;
        if (h1.getLock())
            return false;

        // check that public lock api is available
        (void) &Handle<OpLockable>::getLockOrNotify;
        (void) &Handle<OpLockable>::releaseLock;
        (void) &Handle<OpLockable>::increaseCurrentVersionNoUnlock;

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
    std::string getName() { return "TestHandle"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testName, testId, testLockable, testSubTasks, testCombos
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // __TEST_HANDLE_HPP_

