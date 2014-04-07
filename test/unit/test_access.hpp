#ifndef SG_TEST_ACCESS_HPP_INCLUDED
#define SG_TEST_ACCESS_HPP_INCLUDED

#include <string>

class TestAccess : public TestCase {
    struct OpDefault : public DefaultOptions<OpDefault> {};

    struct MyContrib {
    };

    struct OpContrib : public DefaultOptions<OpContrib> {
        typedef Enable Contributions;
        typedef MyContrib ContributionType;
    };

    struct OpLockable : public DefaultOptions<OpLockable> {
        typedef Enable Lockable;
    };

    struct OpAll : public DefaultOptions<OpAll> {
        typedef Enable Contributions;
        typedef Enable Lockable;
        typedef MyContrib ContributionType;
    };

    static bool testContrib(std::string &name) { name = "testContrib";
        Access<OpContrib> h1, h2;

        if (h1.use_contrib()) return false;
        h1.set_use_contrib(true);
        if (!h1.use_contrib()) return false;
        if (h2.use_contrib()) return false;

        return true;
    }

    static bool testLockable(std::string &name) { name = "testLockable";
        Handle<OpLockable> h1, h2;
        Access<OpLockable> a11(&h1, 0), a12(&h1, 0);
        Access<OpLockable> a21(&h2, 0), a22(&h2, 0);

        if (a11.needs_lock()) return false;

        // if not needslock -- always succeeds
        if (!a11.get_lock()) return false;
        if (!a11.get_lock()) return false;

        a11.set_required_quantity(1);
        if (!a11.needs_lock()) return false;
        if (a12.needs_lock()) return false;

        // now needs lock -- first grab should succeed, second fail
        if (!a11.get_lock()) return false;
        if (a11.get_lock()) return false;


        // different access pointing to same handle should also fail
        // but only if it needs locks
        if (!a12.get_lock()) return false;
        if (!a12.get_lock()) return false;

        // now should fail as a11 already has the lock
        a12.set_required_quantity(1);
        if (a12.get_lock()) return false;

        return true;
    }

    static bool testCombos(std::string &name) { name = "testCombos";
        Access<OpAll> h1;
        return true;
    }

public:

    std::string get_name() { return "TestAccess"; }

    testfunction *get(size_t &numTests) {
        static testfunction tests[] = {
            testContrib, testLockable, testCombos
        };
        numTests = sizeof(tests)/sizeof(testfunction);
        return tests;
    }
};

#endif // SG_TEST_ACCESS_HPP_INCLUDED
