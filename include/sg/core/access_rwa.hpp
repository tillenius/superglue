#ifndef SG_ACCESS_RWA_HPP_INCLUDED
#define SG_ACCESS_RWA_HPP_INCLUDED

namespace sg {

class ReadWriteAdd {
public:
    enum Type { read = 0, add, write, num_accesses };
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

} // namespace sg

#endif // SG_ACCESS_RWA_HPP_INCLUDED
