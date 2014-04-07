#include "sg/superglue.hpp"

class TestCase {
public:
    typedef bool (*testfunction)(std::string &name);

    virtual testfunction *get(size_t &num) = 0;
    virtual std::string get_name() = 0;
    virtual ~TestCase() {}
};

#include "unit/test_handle.hpp"
#include "unit/test_access.hpp"
#include "unit/test_schedver.hpp"
#include "unit/test_taskqueue.hpp"
#include "unit/test_taskqueuedeque.hpp"
#include "unit/test_taskqueueprio.hpp"
#include "unit/test_tasks.hpp"
#include "unit/test_locks.hpp"
#include "unit/test_listqueue.hpp"
#include "unit/test_rwc.hpp"
#include "unit/test_subtasks.hpp"

int main(int argc, char *argv[]) {
    size_t numTests = 0;
    size_t numSuccess = 0;

    TestCase *modules[] = {
        new TestHandle(),
        new TestAccess(),
        new TestSchedulerVer(),
        new TestTaskQueue(),
        new TestTaskQueueDeque(),
        new TestTaskQueuePrio(),
        new TestTasks(),
        new TestLocks(),
        new TestListQueue(),
        new TestRWC(),
        new TestSubtasks()
    };

    for (size_t i = 0; i < sizeof(modules)/sizeof(TestCase*); ++i) {

        size_t numCases = 0;
        TestCase::testfunction *tests = modules[i]->get(numCases);
        numTests += numCases;

        std::vector<std::string> failed;
        std::string testname;

        std::cout << modules[i]->get_name() << std::flush;
        for (size_t j = 0; j < numCases; ++j) {
            if (tests[j](testname))
                ++numSuccess;
            else
                failed.push_back(testname);
        }

        if (!failed.empty()) {
            std::cout << " FAILED: " << modules[i]->get_name() << " [" << failed[0];
            for (size_t j = 1; j < failed.size(); ++j)
                std::cout << ", " << failed[j];
            std::cout << "]" << std::endl;
        }
        else
            std::cout << " OK" << std::endl;
        delete modules[i];
    }

    std::cout << numSuccess << "/" << numTests << std::endl;
    return 0;
}
