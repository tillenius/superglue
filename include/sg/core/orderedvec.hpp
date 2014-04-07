#ifndef SG_ORDEREDVEC_HPP_INCLUDED
#define SG_ORDEREDVEC_HPP_INCLUDED

namespace sg {

template<typename Key, typename Value>
struct elem_t {
    Key key;
    Value value;
    elem_t(Key key_) : key(key_) {}
    bool operator<(const elem_t &rhs) const {
        return key < rhs.key;
    }
};

template<typename deque_t, typename Key, typename Value>
class ordered_vec_t {
    typedef elem_t<Key, Value> element_t;
private:
    deque_t array;
public:
    Key first_key() { return array[0].key; }
    Value pop_front() {
        Value value(array[0].value);
        array.pop_front();
        return value;
    }

    Value &operator[](Key key) {
        // Want to check last item, so first we must make sure such item exist.
        if (array.empty()) {
            array.push_back(element_t(key));
            return array[0].value;
        }

        // Check last item first, since that is the expected location
        const element_t &last( array[array.size()-1] );
        if (last.key == key)
            return array[array.size()-1].value;
        if (last.key < key) {
            array.push_back(element_t(key));
            return array[array.size()-1].value;
        }

        // Binary search to find the version
        typename deque_t::iterator iter = lower_bound(array.begin(), array.end(), key);
        // iter must be valid, cannot be array.end() as we already checked the last element

        if (iter->key == key)
            return iter->value;

        // New version, but not at the end.
        // An expensive insert is done here, but it should be a rare case since
        // requests of versions are expected (but not required) to come in order.
        return array.insert(iter, element_t(key))->value;
    }

    bool empty() const { return array.empty(); }
};

} // namespace sg

#endif // SG_ORDEREDVEC_HPP_INCLUDED
