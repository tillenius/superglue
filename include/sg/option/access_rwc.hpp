#ifndef SG_ACCESS_RWC_HPP_INCLUDED
#define SG_ACCESS_RWC_HPP_INCLUDED

namespace sg {

class ReadWriteConcurrent {
public:
    enum Type { read = 0, write, concurrent, num_accesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

template<> struct ReadWriteConcurrent::AccessType<ReadWriteConcurrent::concurrent> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 0 };
};

} // namespace sg

#endif // SG_ACCESS_RWC_HPP_INCLUDED
