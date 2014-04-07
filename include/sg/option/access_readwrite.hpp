#ifndef SG_ACCESS_READWRITE_HPP_INCLUDED
#define SG_ACCESS_READWRITE_HPP_INCLUDED

namespace sg {

// =====================================================================
// ReadWrite
// =====================================================================
class ReadWrite {
public:
    enum Type { read = 0, write, num_accesses };
    template<int n> struct AccessType {};
};

template<> struct ReadWrite::AccessType<ReadWrite::read> {
    enum { commutative = 1 };
    enum { exclusive = 0 };
    enum { readonly = 1 };
};

template<> struct ReadWrite::AccessType<ReadWrite::write> {
    enum { commutative = 0 };
    enum { exclusive = 1 };
    enum { readonly = 0 };
};

} // namespace sg

#endif // SG_ACCESS_READWRITE_HPP_INCLUDED
