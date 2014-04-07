#ifndef SG_ACCESSUTIL_HPP_INCLUDED
#define SG_ACCESSUTIL_HPP_INCLUDED

namespace sg {

template<class Options>
class AccessUtil {
    typedef typename Options::AccessInfoType AccessInfo;

    template<int n, template<typename T> class Predicate, bool stop = (n==0)>
    struct Aux {
        static bool check(int type) {
            typedef typename AccessInfo::template AccessType<n-1> AccessType;
            if (type == n-1)
                return Predicate<AccessType>::result;
            else
                return Aux<n-1, Predicate>::check(type);
        }
    };

    template<int n, template<typename T> class Predicate>
    struct Aux<n, Predicate, true> {
        // should never be called, but required for compilation
        static bool check(int) { return false; }
    };

    template<int n, template<typename T> class Predicate, bool stop = (n==0)>
    struct AnyTypeAux {
        typedef typename AccessInfo::template AccessType<n-1> AccessType;
        enum { result = (Predicate<AccessType>::result == 1) || (AnyTypeAux<n-1, Predicate>::result == 1) };
    };
    template<int n, template<typename T> class Predicate>
    struct AnyTypeAux<n, Predicate, true> {
        enum { result = 0 };
    };

public:

    template<typename T>
    struct NeedsLockPredicate {
        enum { result = ((T::exclusive == 1) && (T::commutative == 1)) ? 1 : 0 };
    };

    template<typename T>
    struct ConcurrentPredicate {
        enum { result = ((T::exclusive == 0) && (T::commutative == 1)) ? 1 : 0 };
    };

    template<typename T>
    struct CommutativePredicate {
        enum { result = T::commutative };
    };

    template<typename T>
    struct ReadOnlyPredicate {
        enum { result = T::readonly };
    };

    template< template<typename T> class Predicate >
    struct AnyType {
        enum { result = AnyTypeAux<AccessInfo::num_accesses, Predicate>::result };
    };

    static bool needs_lock(int type) {
        return Aux<AccessInfo::num_accesses, NeedsLockPredicate>::check(type);
    }

    static bool concurrent(int type) {
        return Aux<AccessInfo::num_accesses, ConcurrentPredicate>::check(type);
    }

    static bool commutative(int type) {
        return Aux<AccessInfo::num_accesses, CommutativePredicate>::check(type);
    }

    static bool readonly(int type) {
        return Aux<AccessInfo::num_accesses, ReadOnlyPredicate>::check(type);
    }
};

} // namespace sg

#endif // SG_ACCESSUTIL_HPP_INCLUDED
