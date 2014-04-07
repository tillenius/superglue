#ifndef SG_SCHEDULERVER_HPP_INCLUDED
#define SG_SCHEDULERVER_HPP_INCLUDED

#include "sg/core/access_rwa.hpp" // specialize for ReadWriteAdd
#include "sg/core/spinlock.hpp"

#include <cstring> // memset
#include <algorithm> // max_element

namespace sg {

namespace detail {

    // store next version for each type
    template<typename Options, typename AccessInfo>
    class SchedulerVersionImpl {
    private:
        typedef typename Options::version_t version_t;

        version_t required_version[AccessInfo::num_accesses];

        // increase scheduler version on dynamic type
        template<int n, bool stop = (n==0)>
        struct IncreaseAux {
            static void increase(int type, version_t *required_version, version_t next_ver) {
                typedef typename AccessInfo::template AccessType<n-1> AccessType;

                IncreaseAux<n-1>::increase(type, required_version, next_ver);

                // set next-required-version to next version, except if the type is commutative,
                // in which case the next-required-version for the type should not be modified.
                if (type != n-1 || AccessType::commutative == 0)
                    required_version[n-1] = next_ver;
            }
        };
        template<int n>
        struct IncreaseAux<n, true> {
            static void increase(int /*type*/, version_t * /*required_version*/, version_t /*next_ver*/) {}
        };

    public:
        SchedulerVersionImpl() {
            std::memset(required_version, 0, sizeof(required_version));
        }

        version_t next_version() {
            return *std::max_element(required_version, required_version + AccessInfo::num_accesses)+1;
        }

        version_t schedule(int type) {
            const version_t ver = required_version[type];
            IncreaseAux<AccessInfo::num_accesses>::increase(type, required_version, next_version());
            return ver;
        }
    };

    // Specialize SchedulerVersionImpl for default ReadWriteAdd
    // to use 2 counters instead of 3
    template<typename Options>
    class SchedulerVersionImpl<Options, ReadWriteAdd> {
    private:
        typedef typename Options::version_t version_t;
        version_t required_version[2];

    public:
        SchedulerVersionImpl() {
            required_version[0] = required_version[1] = 0;
        }

        version_t next_version() {
            return std::max(required_version[0], required_version[1])+1;
        }

        version_t schedule(int type) {
            const version_t next_ver = next_version();
            switch (type) {
            case ReadWriteAdd::read:
                required_version[ReadWriteAdd::add] = next_ver;
                return required_version[ReadWriteAdd::read];
            case ReadWriteAdd::add:
                required_version[ReadWriteAdd::read] = next_ver;
                return required_version[ReadWriteAdd::add];
            }
            required_version[0] = required_version[1] = next_ver;
            return next_ver-1;
        }
    };

    // ============================================================================
    // SchedulerVersion
    // ============================================================================
    template<typename Options, typename T = typename Options::ThreadSafeSubmit> class SchedulerVersion;

    template<typename Options>
    class SchedulerVersion<Options, typename Options::Enable>
     : public detail::SchedulerVersionImpl<Options, typename Options::AccessInfoType> {
        typedef typename detail::SchedulerVersionImpl<Options, typename Options::AccessInfoType> parent;
        typedef typename Options::version_t version_t;
    private:
        SpinLock lock;
    public:
        version_t schedule(int type) {
            SpinLockScoped l(lock);
            return parent::schedule(type);
        }
    };

    template<typename Options>
    class SchedulerVersion<Options, typename Options::Disable>
     : public detail::SchedulerVersionImpl<Options, typename Options::AccessInfoType> {
        typedef typename detail::SchedulerVersionImpl<Options, typename Options::AccessInfoType> parent;
        typedef typename Options::version_t version_t;
    public:
        version_t schedule(int type) { return parent::schedule(type); }
    };

} // namespace detail

template<typename Options> class SchedulerVersion : public detail::SchedulerVersion<Options> {};

} // namespace sg

#endif // SG_SCHEDULERVER_HPP_INCLUDED
