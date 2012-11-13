#ifndef __ACCESSUTIL_HPP__
#define __ACCESSUTIL_HPP__

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

    template< template<typename T> class Predicate >
    struct AnyType {
        enum { result = AnyTypeAux<AccessInfo::numAccesses, Predicate>::result };
    };

    static bool needsLock(int type) {
        return Aux<AccessInfo::numAccesses, NeedsLockPredicate>::check(type);
    }

    static bool concurrent(int type) {
        return Aux<AccessInfo::numAccesses, ConcurrentPredicate>::check(type);
    }
};

#endif // __ACCESSUTIL_HPP__
