#ifndef __SCHEDULERVER_HPP__
#define __SCHEDULERVER_HPP__

#include "accessutil.hpp"

#include <cstring> // memset
#include <algorithm> // max_elements

namespace detail {

// overridden in accesstypes.hpp

    // store next version for each type
    template<typename Options, typename AccessInfo>
    class SchedulerVersionImpl {
    private:
        typedef typename Options::version_t version_t;

        version_t requiredVersion[AccessInfo::numAccesses];

        // increase scheduler version on dynamic type
        template<int n, bool stop = (n==0)>
        struct IncreaseAux {
            static void increase(int type, version_t *requiredVersion, version_t nextVer) {
                typedef typename AccessInfo::template AccessType<n-1> AccessType;

                IncreaseAux<n-1>::increase(type, requiredVersion, nextVer);

                // set next-required-version to next version, except if the type is commutative,
                // in which case the next-required-version for the type should not be modified.
                if (type != n-1 || AccessType::commutative == 0)
                    requiredVersion[n-1] = nextVer;
            }
        };
        template<int n>
        struct IncreaseAux<n, true> {
            static void increase(int /*type*/, version_t * /*requiredVersion*/, version_t /*nextVer*/) {}
        };

    public:
        SchedulerVersionImpl() {
            std::memset(requiredVersion, 0, sizeof(requiredVersion));
        }

        version_t nextVersion() {
            return *std::max_element(requiredVersion, requiredVersion + AccessInfo::numAccesses)+1;
        }

        version_t schedule(int type) {
            const version_t ver = requiredVersion[type];
            IncreaseAux<AccessInfo::numAccesses>::increase(type, requiredVersion, nextVersion());
            return ver;
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

#endif // __SCHEDULERVER_HPP__
