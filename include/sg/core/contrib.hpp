#ifndef SG_CONTRIB_HPP_INCLUDED
#define SG_CONTRIB_HPP_INCLUDED

#include <algorithm>
#include <new>

namespace sg {

template<typename T> 
class Contribution {
public:
    size_t size;
    size_t versions;
    T *dst;

private:
    Contribution(T *dst_, size_t size_) : size(size_), versions(1), dst(dst_) {}

public:
    static void merge_impl(T *src, T *dst, size_t size) {
        for (size_t i(0); i < size; ++i)
            dst[i] += src[i];
    }

    size_t get_versions() const { return versions; }
    void merge(Contribution *temp) {
        assert(size == temp->size);
        versions += temp->versions;
        merge_impl(get_data(*temp), get_data(*this), size);
        free(temp);
    }
    static size_t data_offset() {
        // align upwards to multiple of 4
        return (sizeof(Contribution<T>) + 0x3) & ~static_cast<size_t>(0x3); 
    }

    static T *get_data(Contribution<T> &c) {
        return (T*) (((char *) &c) + data_offset());
    }

    static void apply_and_free(Contribution<T> *c) {
        merge_impl(get_data(*c), c->dst, c->size);
        free(c);
    }

    static Contribution<T> *allocate(size_t size, T *dest) {
        char *c = new char[data_offset() + size*sizeof(T)];
        return new (c) Contribution(dest, size);
    }
    static void free(Contribution<T> *c) {
        // Note that the Contribution<T> destructor is never called
        delete [] (char *) c;
    }
};

} // namespace sg

#endif // SG_CONTRIB_HPP_INCLUDED
