#ifndef SG_OPENMPUTIL_HPP_INCLUDED
#define SG_OPENMPUTIL_HPP_INCLUDED

#include <omp.h>

namespace sg {

class OpenMPUtil {
public:
    static int get_num_cpus() { return omp_get_num_threads(); }
    static int get_current_thread_id() { return omp_get_thread_num(); }
};

} // namespace sg

#endif // SG_OPENMPUTIL_HPP_INCLUDED
