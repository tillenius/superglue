#include "sg/superglue.hpp"
#include "sg/core/contrib.hpp"
#include "sg/option/instr_trace.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

using namespace std;

struct vector_t {
    double x[3];
    void operator +=(const vector_t &y) {
        const double *z(y.x);
        x[0] += z[0];
        x[1] += z[1];
        x[2] += z[2];
    }
};

struct particle_t {
    double x[3];
    double dx[3];
};

// ==========================================================================
// Task Library Options
//
// Set task library options:
//   * Enable logging (generate execution trace)
//   * Handles can be locked
//   * Contributions are used (renaming for add-accesses)
// ==========================================================================
struct Options : public DefaultOptions<Options> {
    typedef Trace<Options> Instrumentation;
    typedef Enable Contributions;
    typedef Contribution<vector_t> ContributionType;
    typedef Enable TaskName;
};

typedef Handle<Options> handle_t;
typedef Access<Options> access_t;

const double dt = 0.01;
const double EPSILON = 1.0;
const double SIGMA = 1.0;
const double LJ_CONST_A = -24.0*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*EPSILON;
const double LJ_CONST_B = -48.0*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*SIGMA*EPSILON;

particle_t *globalptr;

//======================================================
// "kernels"
//======================================================

inline static void eval_force_tk(particle_t *p0, particle_t *p1, vector_t *force) {
    const double dx = p0->x[0] - p1->x[0];
    const double dy = p0->x[1] - p1->x[1];
    const double dz = p0->x[2] - p1->x[2];
    const double r2 = dx*dx + dy*dy + dz*dz;
    const double r4 = r2 * r2;
    const double r8 = r4 * r4;
    const double r14 = r8 * r4 * r2;
    const double c = LJ_CONST_A / r8 - LJ_CONST_B / r14;
    force->x[0] = c * dx;
    force->x[1] = c * dy;
    force->x[2] = c * dz;
}

inline static void init_particle_tk(particle_t *p, int i, int num_particles) {
    int s = (int) pow(num_particles, 1.0/3.0);
    int px = i/(s*s);
    int py = (i-px*s*s)/s;
    int pz = (i-px*s*s-py*s);

    const double magnitude = 1.0;
    const double distance = 1.0 * SIGMA;
    const double theta = rand() * 6.28318531 / RAND_MAX;
    p->dx[0] = sin(theta) * magnitude;
    p->dx[1] = cos(theta) * magnitude;
    p->dx[2] = 0.0;
    p->x[0] = px * distance;
    p->x[1] = py * distance;
    p->x[2] = pz * distance;
}

inline static void step_tk(particle_t *p, vector_t *a) {
    const double c1 = 0.5 * dt;
    const double c2 = c1 * dt;

    p->dx[0] += c1 * a->x[0];
    p->dx[1] += c1 * a->x[1];
    p->dx[2] += c1 * a->x[2];

    p->x[0] += dt * p->dx[0] + c2 * a->x[0];
    p->x[1] += dt * p->dx[1] + c2 * a->x[1];
    p->x[2] += dt * p->dx[2] + c2 * a->x[2];

    p->dx[0] += c1 * a->x[0];
    p->dx[1] += c1 * a->x[1];
    p->dx[2] += c1 * a->x[2];

    a->x[0] = a->x[1] = a->x[2] = 0.0;
}


//======================================================
// "task payload"
//======================================================

static void task_eval_within(particle_t *particles, vector_t *forces, size_t bsz) {
    for (size_t i = 0; i < bsz; ++i) {
        vector_t tmp;
        tmp.x[0] = tmp.x[1] = tmp.x[2] = 0.0;
        for (size_t j = i + 1; j < bsz; ++j) {
            vector_t f;
            eval_force_tk(&particles[i], &particles[j], &f);
            tmp.x[0] += f.x[0];
            tmp.x[1] += f.x[1];
            tmp.x[2] += f.x[2];
            forces[j].x[0] -= f.x[0];
            forces[j].x[1] -= f.x[1];
            forces[j].x[2] -= f.x[2];
        }
        forces[i].x[0] += tmp.x[0];
        forces[i].x[1] += tmp.x[1];
        forces[i].x[2] += tmp.x[2];
    }
}

template<typename T>
struct ScopedContribProxy {
    access_t &a;
    union {
        Contribution<T> *contrib;
        T *src;
    };
    ScopedContribProxy(access_t &a_, T *dest, size_t size) : a(a_) {
        if (a.use_contrib()) {
            contrib = a.get_handle()->get_contribution();
            if (contrib != NULL)
                return;
            contrib = Contribution<T>::allocate(size, dest);
            fill((double *) Contribution<T>::get_data(*contrib),
                 (double *) (Contribution<T>::get_data(*contrib)+size), 0.0);
        }
        else
            src = dest;
    }
    ~ScopedContribProxy() {
        if (a.use_contrib())
            a.get_handle()->add_contribution(contrib);
    }
    T *getAddress() {
        if (a.use_contrib())
            return Contribution<T>::get_data(*contrib);
        else
            return src;
    }
};

static void task_eval_between(particle_t *p0, particle_t *p1, vector_t *f0_, vector_t *f1_,
                              access_t a0, access_t a1, size_t bsz) {
    ScopedContribProxy<vector_t> force0(a0, f0_, bsz);
    ScopedContribProxy<vector_t> force1(a1, f1_, bsz);
    vector_t *f0 = force0.getAddress();
    vector_t *f1 = force1.getAddress();
    for (size_t i = 0; i < bsz; ++i) {
        vector_t tmp;
        tmp.x[0] = tmp.x[1] = tmp.x[2] = 0.0;
        for (size_t j = 0; j < bsz; ++j) {
            vector_t f;
            eval_force_tk(&p0[i], &p1[j], &f);
            tmp.x[0] += f.x[0];
            tmp.x[1] += f.x[1];
            tmp.x[2] += f.x[2];
            f1[j].x[0] -= f.x[0];
            f1[j].x[1] -= f.x[1];
            f1[j].x[2] -= f.x[2];
        }
        f0[i].x[0] += tmp.x[0];
        f0[i].x[1] += tmp.x[1];
        f0[i].x[2] += tmp.x[2];
    }
}

static void task_step(particle_t *particles, vector_t *forces, size_t bsz) {
    for (size_t i = 0; i < bsz; ++i)
        step_tk(&particles[i], &forces[i]);
}

//======================================================
// "task classes"
//======================================================

class TimeStepTask : public Task<Options, 2> {
private:
    particle_t *p0_;
    vector_t *f0_;
    size_t slice_size_;

public:
    TimeStepTask(particle_t *p0, handle_t &hp0,
                 vector_t *f0, handle_t &hf0,
                 size_t slice_size) {
        register_access(ReadWriteAdd::read, hf0);
        register_access(ReadWriteAdd::add, hp0);
        p0_ = p0; f0_ = f0;
        slice_size_ = slice_size;
    }
    virtual void run() {
        task_step(p0_, f0_, slice_size_);
    }
    virtual string get_name() {
        stringstream ss;
        ss << "step " << (p0_ - globalptr)/slice_size_;
        return ss.str();
    }
};

class EvalWithinTask : public Task<Options, 2> {
private:
    particle_t *p0_;
    vector_t *f0_;
    size_t slice_size_;

public:
    EvalWithinTask(particle_t *p0, handle_t &hp0,
                   vector_t *f0, handle_t &hf0,
                   size_t slice_size) {
        register_access(ReadWriteAdd::read, hp0);
        register_access(ReadWriteAdd::add, hf0);
        p0_ = p0; f0_ = f0;
        slice_size_ = slice_size;
    }
    virtual void run() {
        task_eval_within(p0_, f0_, slice_size_);
    }
    virtual string get_name() {
        stringstream ss;
        ss << "evalWithin " << (p0_ - globalptr)/slice_size_;
        return ss.str();
    }

};

class EvalBetweenTask : public Task<Options, 4> {
private:
    particle_t *p0_, *p1_;
    vector_t *f0_, *f1_;
    size_t slice_size_;

public:
    EvalBetweenTask(particle_t *p0, handle_t &hp0,
                    particle_t *p1, handle_t &hp1,
                    vector_t *f0, handle_t &hf0,
                    vector_t *f1, handle_t &hf1,
                    size_t slice_size) {
        register_access(ReadWriteAdd::read, hp0);
        register_access(ReadWriteAdd::read, hp1);
        register_access(ReadWriteAdd::add, hf0);
        register_access(ReadWriteAdd::add, hf1);
        p0_ = p0; p1_ = p1;
        f0_ = f0; f1_ = f1;
        slice_size_ = slice_size;
    }

    virtual bool can_run_with_contribs() { return true; }

    virtual void run() {
        task_eval_between(p0_, p1_, f0_, f1_, Task<Options, 4>::get_access(2),
                          Task<Options, 4>::get_access(3), slice_size_);
    }
    virtual string get_name() {
        stringstream ss;
        ss << "evalBetween "
                << (p0_ - globalptr)/slice_size_ << " "
                << (p1_ - globalptr)/slice_size_;
        return ss.str();
    }
};

//======================================================
// initialize particles and forces
//======================================================
void init(particle_t *particles, vector_t *forces, const size_t num_particles) {
    size_t i;

    for (i = 0; i < num_particles; ++i)
        init_particle_tk(&particles[i], i, num_particles);

    for (i = 0; i < num_particles; ++i)
        forces[i].x[0] = forces[i].x[1] = forces[i].x[2] = 0.0;
}

//======================================================
// evaluate all forces
//======================================================
void eval_force(SuperGlue<Options> &sg,
        particle_t *particles, handle_t *part,
        vector_t *forces, handle_t *forc,
        const size_t block_size, const size_t num_blocks) {

    for (size_t i = 0; i < num_blocks; ++i) {
        sg.submit(new EvalWithinTask(&particles[i*block_size], part[i],
                                     &forces[i*block_size], forc[i],
                                     block_size));
    }
    for (size_t i = 0; i < num_blocks; ++i) {
        for (size_t j = i + 1; j < num_blocks; ++j)
            sg.submit(new EvalBetweenTask(&particles[i*block_size], part[i],
                                          &particles[j*block_size], part[j],
                                          &forces[i*block_size], forc[i],
                                          &forces[j*block_size], forc[j],
                                          block_size));
    }
}

//======================================================
// take a time step
//======================================================
void step(SuperGlue<Options> &sg,
        particle_t *particles, handle_t *part,
        vector_t *forces, handle_t *forc,
        const size_t block_size, const size_t num_blocks) {
    size_t i;

    for (i = 0; i < num_blocks; ++i) {
        sg.submit(new TimeStepTask(&particles[i*block_size], part[i],
                                   &forces[i*block_size], forc[i],
                                   block_size));
    }
}

//======================================================
// run simulation for num_steps time steps
//======================================================
void run(SuperGlue<Options> &sg,
        particle_t *particles, handle_t *part,
        vector_t *forces, handle_t *forc,
        const size_t block_size, const size_t num_blocks,
        const size_t num_steps) {
    size_t i;

    for (i = 0; i < num_steps; ++i) {
        eval_force(sg, particles, part, forces, forc, block_size, num_blocks);
        step(sg, particles, part, forces, forc, block_size, num_blocks);
    }
}

//======================================================
// reference implementation
//======================================================
void reference(particle_t *particles, vector_t *forces,
        const size_t num_particles, const size_t num_steps) {
    vector_t f;
    size_t s, i, j;

    for (s = 0; s < num_steps; ++s) {
        for (i = 0; i < num_particles; ++i) {
            vector_t tmp;
            tmp.x[0] = tmp.x[1] = tmp.x[2] = 0.0;
            for (j = i+1; j < num_particles; ++j) {
                eval_force_tk(&particles[i], &particles[j], &f);
                tmp.x[0] += f.x[0];
                tmp.x[1] += f.x[1];
                tmp.x[2] += f.x[2];
                forces[j].x[0] -= f.x[0];
                forces[j].x[1] -= f.x[1];
                forces[j].x[2] -= f.x[2];
            }
            forces[i].x[0] += tmp.x[0];
            forces[i].x[1] += tmp.x[1];
            forces[i].x[2] += tmp.x[2];
        }
        for (i = 0; i < num_particles; ++i)
            step_tk(&particles[i], &forces[i]);
    }
}

//======================================================
// compare two solutions
//======================================================
void compare(particle_t *p0, particle_t *p1, const size_t num_particles) {
    size_t i;

    for (i = 0; i < num_particles; ++i) {
        const double dx = p0[i].x[0] - p1[i].x[0];
        const double dy = p0[i].x[1] - p1[i].x[1];
        const double dz = p0[i].x[2] - p1[i].x[2];
        const double d2 = dx*dx+dy*dy+dz*dz;

        if (d2 > 1e-6) {
            cerr << "### " << i 
                 << ": ("   << p0[i].x[0] << ", " << p0[i].x[1] << ", " << p0[i].x[2]
                 << ") - (" << p1[i].x[0] << ", " << p1[i].x[1] << ", " << p1[i].x[2]
                 << ") = (" << dx << ", " << dy << ", " << dz << ")" << endl;
        }
    }
}

//======================================================
// print out result
//======================================================
void dump(const particle_t *p, const size_t num_particles) {
    size_t i;

    cout << num_particles << endl;
    for (i = 0; i < num_particles; ++i)
        cout << p[i].x[0] << ", " << p[i].x[1] << ", " << p[i].x[2] << endl;
}

void ref(particle_t *particles, vector_t *forces, const size_t num_particles, const size_t num_steps) {
    // run simulation in serial
    srand(0);
    init(&particles[0], &forces[0], num_particles);

    Time::TimeUnit time_start2 = Time::getTime();
    reference(&particles[0], &forces[0], num_particles, num_steps);
    Time::TimeUnit time_stop2 = Time::getTime();
    cout << "ref: " << time_stop2-time_start2 << endl;
}

void run(particle_t *particles, vector_t *forces,
        const size_t num_particles, const size_t block_size, const size_t num_steps,
        const int num_threads) {
    globalptr = particles; // for debugging

    // run simulation using task library
    srand(0);
    init(&particles[0], &forces[0], num_particles);

    const size_t num_blocks = num_particles/block_size;

    handle_t *part = new handle_t[num_blocks];
    handle_t *forc = new handle_t[num_blocks];
    Time::TimeUnit time_start;
    Time::TimeUnit time_stop;

    SuperGlue<Options> sg(num_threads);
    time_start = Time::getTime();
    run(sg, particles, part, forces, forc, block_size, num_blocks, num_steps);
    sg.barrier();
    time_stop = Time::getTime();

    cout << "#cores=" << sg.get_num_cpus()
         << " #particles=" << num_particles
         << " blocksize=" << block_size
         << " time=" << time_stop-time_start << " cycles"
         << endl;

    delete [] part;
    delete [] forc;
}

int main(int argc, char *argv[]) {

    size_t num_particles, block_size, num_steps;
    if (argc >= 4) {
        num_particles = (size_t) atoi(argv[1]);
        block_size = (size_t) atoi(argv[2]);
        num_steps = (size_t) atoi(argv[3]);
    }
    else {
        cout << "usage: " << argv[0] << " <num_particles> <blocksize> <num_steps> [num_cores]" << endl << endl;
        cout << "Example: " << argv[0] << " 1024 128 4" << endl;
        exit(0);
    }

    int num_threads = -1;
    if (argc >= 5)
        num_threads = atoi(argv[4]);

    particle_t *particles = new particle_t[num_particles];
    vector_t *forces = new vector_t[num_particles];
    particle_t *particles2 = new particle_t[num_particles];

    ref(particles2, forces, num_particles, num_steps);
    run(particles, forces, num_particles, block_size, num_steps, num_threads);
    compare(particles, particles2, num_particles);

    Trace<Options>::dump("execution.log");

    delete [] particles;
    delete [] forces;
    delete [] particles2;
}

