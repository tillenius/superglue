#ifndef __CONTRIB_HPP__
#define __CONTRIB_HPP__

#include <algorithm>
#include <new>

template<typename T> 
class Contribution {
public:
    size_t size;
    size_t versions;
    T *dst;

private:
    Contribution(T *dst_, size_t size_) : size(size_), versions(1), dst(dst_) {}

public:
    static void mergeImpl(T *src, T *dst, size_t size) {
        for (size_t i(0); i < size; ++i)
            dst[i] += src[i];
    }

    size_t getVersions() const { return versions; }
    void merge(Contribution *temp) {
        assert(size == temp->size);
        versions += temp->versions;
        mergeImpl(getData(*temp), getData(*this), size);
        free(temp);
    }
    static size_t dataOffset() {
		// align upwards to multiple of 4
        return (sizeof(Contribution<T>) + 0x3) & ~static_cast<size_t>(0x3); 
    }

    static T *getData(Contribution<T> &c) {
        return (T*) (((char *) &c) + dataOffset());
    }

    static void applyAndFree(Contribution<T> *c) {
        mergeImpl(getData(*c), c->dst, c->size);
        free(c);
    }

    static Contribution<T> *allocate(size_t size, T *dest) {
        char *c = new char[dataOffset() + size*sizeof(T)];
        return new (c) Contribution(dest, size);
    }
    static void free(Contribution<T> *c) {
        // Note that the Contribution<T> destructor is never called
        delete [] (char *) c;
    }
};

#endif // __CONTRIB_HPP__
