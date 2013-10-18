//g++ opengl.cpp -lGL -lglut

#ifdef __APPLE__
#include <OPENGL/gl.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace std;

struct Entry {
    unsigned long long proc;
    unsigned long long thread;
    unsigned long long start;
    unsigned long long length;
    string name;
    Entry() {
      proc = thread = start = length = 0;
    }
};

struct MyVertex {
    double x, y;
    MyVertex(double x_, double y_)
     : x(x_), y(y_) {}
};

struct MyColor {
    GLubyte r, g, b;
    MyColor() {}
    MyColor(GLubyte r_, GLubyte g_, GLubyte b_)
     : r(r_), g(g_), b(b_) {}
};

std::vector<MyVertex> verticesA;
std::vector<MyVertex> verticesB;
std::vector<MyColor> colA;
std::vector<MyColor> colB;
vector<double> rowpos;
vector< pair< unsigned long long, unsigned long long > > rowdata;
vector< vector< double > > procthr2coord;

vector< vector< vector< Entry > > > tasksperproc;

GLfloat m_pos[3];
int window_width = 0;
int window_height = 0;
int m_firstx = 0;
int m_firsty = 0;
int curr_x = 0;
int curr_y = 0;
int m_oldx = 0;
int m_oldy = 0;
bool drag = false;

double viewx0 = 0.0, viewx1 = 1.0;
double viewy0 = 0.0, viewy1 = 1.0;
double scalex = 0.9, scaley = 0.9;

bool selection = false;
double selectionx0, selectionx1, selectiony;
unsigned long long g_selrowidx = 0;
unsigned long long g_seltask = 0;
double g_seltime;
std::map<std::string, size_t> g_tasknames;

void getCoords(int x, int y, double &xx, double &yy) {
    xx = x/(double) window_width;
    yy = (window_height - y)/(double) window_height;

    xx *= (viewx1-viewx0)/scalex;
    yy *= (viewy1-viewy0)/scaley;
    xx += viewx0 - m_pos[0];
    yy += viewy0 - m_pos[1];
}

bool startsWith(const std::string &value1, const std::string &value2) {
    return value1.find(value2) == 0;
}

void getColor(Entry &e, MyColor &c) {
#ifdef COLORS_HARDCODED
    if (startsWith(e.name, "MPI_RECV"))                  c = MyColor(128,0,0);
    else if (startsWith(e.name, "op_ass_single_task"))   c = MyColor(0,128,0);
    else if (startsWith(e.name, "op_ass_task_dist"))     c = MyColor(128,0,128);
    else if (startsWith(e.name, "MPI_BARRIER"))          c = MyColor(0,0,128);
    else if (startsWith(e.name, "MPI_ISEND"))            c = MyColor(0,128,128);
    else c = MyColor(128, 128, 128);
#else
    MyColor cols[] = { 
        MyColor(128,  0,  0), MyColor(  0,128,  0), MyColor(  0,  0,128), 
        MyColor(128,128,  0), MyColor(128,  0,128), MyColor(  0,128,128),
        MyColor(128,128,128), MyColor(  0,  0,  0),
        MyColor(128, 80,  0), MyColor( 80,128,  0), MyColor(  0, 80,128), 
        MyColor(128,  0, 80), MyColor(  0,128, 80), MyColor( 80,  0,128), 
        MyColor(128,128, 80), MyColor(128, 80,128), MyColor( 80,128,128)
        
    };
    std::string s = e.name;
    size_t spos = s.find(' ');
    if (spos != std::string::npos)
        s = s.substr(0, spos);

    size_t idx = g_tasknames[s] % (sizeof(cols)/sizeof(MyColor));
    c = cols[idx];
#endif
}

void setup() {

    m_pos[0] = 0.0f;
    m_pos[1] = 0.0f;
    m_pos[2] = -1.0f;

    viewx0 = 0;
    viewy0 = 0;
    viewx1 = 0.0;
    viewy1 = 0.0;

    size_t row = 0;
    double rowheight = 1.0;
    double barheight = 0.8;
    
    procthr2coord.resize(tasksperproc.size());
    for (size_t proc = 0; proc < tasksperproc.size(); ++proc) {
        procthr2coord[proc].resize(tasksperproc[proc].size());
        for (size_t thread = 0; thread < tasksperproc[proc].size(); ++thread, ++row) {

            double y = row * rowheight + 0.5;
            double y0 = y - barheight/2.0;
            double y1 = y + barheight/2.0;
            rowpos.push_back(y);
            rowdata.push_back(make_pair(proc, thread));
            procthr2coord[proc][thread] = y;

            for (size_t i = 0; i < tasksperproc[proc][thread].size(); ++i) {
                double start = tasksperproc[proc][thread][i].start;
                double end = tasksperproc[proc][thread][i].start + tasksperproc[proc][thread][i].length;

                if (end > viewx1) viewx1 = end;
                if (y1 > viewy1)  viewy1 = y1;

                MyColor col;
                getColor(tasksperproc[proc][thread][i],col);

                verticesA.push_back(MyVertex(start, y0));
                verticesA.push_back(MyVertex(end, (y0+y1)/2));
                verticesA.push_back(MyVertex(start, y1));
                colA.push_back(col);
                colA.push_back(col);
                colA.push_back(col);

            }
        }
    }

    m_pos[0] = 0.05f * viewx1;
    m_pos[1] = 0.05f * viewy1;
}


void paint() {
    glutPostRedisplay();
}

void doPaint() {
    glClearColor(1,1,1,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glScalef(scalex, scaley, 0.0);
    glTranslatef(m_pos[0], m_pos[1], m_pos[2]);

    if (selection) {
        glColor3ub(128, 255, 80);
        glRectd(selectionx0, selectiony-0.50, selectionx1, selectiony+0.50);
        glColor3ub(200, 200, 200);

        double xx, yy, yy2;
        getCoords(0, 0, xx, yy);
        getCoords(0, window_height, xx, yy2);

        glBegin(GL_LINES);
        glVertex3d(selectionx0, yy, 0.0);
        glVertex3d(selectionx0, yy2, 0.0);
        glVertex3d(selectionx1, yy, 0.0);
        glVertex3d(selectionx1, yy2, 0.0);
        glEnd();
    }

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < verticesA.size(); i += 3) {
        glColor3ub(colA[i].r, colA[i].g, colA[i].b);
        glVertex3d(verticesA[i+0].x, verticesA[i+0].y, 0.0);
        glVertex3d(verticesA[i+1].x, verticesA[i+1].y, 0.0);
        glVertex3d(verticesA[i+2].x, verticesA[i+2].y, 0.0);
    }
    glEnd();

    glBegin(GL_LINES);
    for (size_t i = 0; i < verticesA.size(); i += 3) {
        glColor3ub(colA[i].r, colA[i].g, colA[i].b);
        glVertex3d(verticesA[i+0].x, verticesA[i+0].y, 0.0);
        glVertex3d(verticesA[i+1].x, verticesA[i+1].y, 0.0);
        glVertex3d(verticesA[i+1].x, verticesA[i+1].y, 0.0);
        glVertex3d(verticesA[i+2].x, verticesA[i+2].y, 0.0);
        glVertex3d(verticesA[i+2].x, verticesA[i+2].y, 0.0);
        glVertex3d(verticesA[i+0].x, verticesA[i+0].y, 0.0);
    }
    glEnd();

    // draw selection

    if (selection) {
        glColor3ub(128, 255, 80);
        glBegin(GL_LINES);
        glVertex3d(selectionx0, selectiony+.50, 0.0);
        glVertex3d(selectionx0, selectiony-.50, 0.0);
        glVertex3d(selectionx1, selectiony+.50, 0.0);
        glVertex3d(selectionx1, selectiony-.50, 0.0);
        glEnd();
    }

    glFlush();
    glutSwapBuffers();
}

void resize(int width, int height) {

    if (height == 0)
        height = 1;

    window_width = width;
    window_height = height;

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(viewx0, viewx1, viewy0, viewy1, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

string format(unsigned long long a) {
    stringstream ss;
    ss << a;
    string r = ss.str();
    if (r.size() > 6)
        r = r.substr(0, r.size()-6) + "." + r.substr(r.size()-6);
    else {
        while (r.size() < 6)
            r = "0" + r;
        r = "0." + r;
    }
    return r;
}

void select_task(unsigned long long proc, unsigned long long thread, size_t pos) {
    vector<Entry> &tasks(tasksperproc[proc][thread]);
    g_seltask = pos;
    selection = true;

    cout << "(" << proc << ", " << thread
            << ") [ " << format(tasks[pos].start)
            << " " << format(tasks[pos].start+tasks[pos].length)
            << " = " << format(tasks[pos].length) << " ]"
            << " name=[" << tasks[pos].name << "]"
            << endl;

    selectiony = procthr2coord[proc][thread];
    selectionx0 = tasks[pos].start;
    selectionx1 = tasks[pos].start + tasks[pos].length;
    paint();
}

void find_task(size_t rowidx, double time,
               unsigned long long &proc, unsigned long long &thread, size_t &pos) {
    if (rowdata.size() <= rowidx)
        return;

    proc = rowdata[rowidx].first;
    thread = rowdata[rowidx].second;

    vector<Entry> &tasks(tasksperproc[proc][thread]);

    size_t a = 0;
    size_t b = tasks.size()-1;
    pos = (a+b)/2;

    while (b >= a) {
        pos = (a+b)/2;
        if (tasks[pos].start > time) {
            if (pos == 0)
                break;
            b = pos-1;
            continue;
        }
        if (tasks[pos].start + tasks[pos].length < time) {
            a = pos+1;
            continue;
        }
        break;
    }
    if (pos > 0) {
        if (fabs(time - tasks[pos-1].start + tasks[pos-1].length) <
            fabs(time - tasks[pos].start))
            pos = pos - 1;
    }
    if (pos < tasks.size()-1) {
        if (fabs(time - tasks[pos+1].start) <
            fabs(time - (tasks[pos].start+tasks[pos].length)))
            pos = pos + 1;
    }
}

void mouse_release(int x, int y, int btn) {
    if (drag)
        return;
    if (btn != 1)
        return;

    double xx, yy;
    getCoords(x, y, xx, yy);

    size_t bestidx = 0;
    double best = fabs(rowpos[0] - yy);

    for (size_t i = 0; i < rowpos.size(); ++i) {
        double dist = fabs(rowpos[i] - yy);
        if (dist < best) {
            bestidx = i;
            best = dist;
        }
    }
    g_selrowidx = bestidx;

    unsigned long long proc;
    unsigned long long thread;
    size_t pos;
    find_task(g_selrowidx, xx, proc, thread, pos);

    g_seltask = pos;
    g_seltime = xx;

    select_task(proc, thread, pos);
}

void mouse_click(int x, int y, int btn) {
    m_firstx = x;
    m_firsty = y;
    m_oldx = x;
    m_oldy = y;
    drag = false;
}

void mouse_drag(int dx, int dy, int button) {

    double rx = ( (viewx1-viewx0) / (window_width) );
    double ry = ( (viewy1-viewy0) / (window_height) );

    if (button & 1) {
        m_pos[0] += dx*rx/scalex;
        m_pos[1] += dy*ry/scaley;
    }
    else if (button & 2) {
        double xx, yy;
        getCoords(m_firstx, m_firsty, xx, yy);

        scalex *= exp(dx/100.f);
        scaley *= exp(dy/100.f);
        if (scalex < 0.5) scalex = 0.5;
        if (scaley < 0.5) scaley = 0.5;

        double xx2, yy2;
        getCoords(m_firstx, m_firsty, xx2, yy2);
        m_pos[0] += xx2-xx;
        m_pos[1] += yy2-yy;
    }

    double xx, yy;
    getCoords(window_width/2, window_height/2, xx, yy);
    if (xx < viewx0) m_pos[0] += xx-viewx0;
    if (xx > viewx1) m_pos[0] -= viewx1-xx;
    if (yy < viewy0) m_pos[1] += yy-viewy0;
    if (yy > viewy1) m_pos[1] -= viewy1-yy;
}

void mouse_move(int x, int y, int btn) {
    curr_x = x;
    curr_y = y;
    if (btn == 0)
        return;
    int dx = x - m_oldx;
    int dy = y - m_oldy;

    if (!drag && abs(x - m_firstx) < 3 && abs(y - m_firsty) < 3)
        return;

    drag = true;

    mouse_drag(dx, -dy, btn);
    m_oldx = x;
    m_oldy = y;
}

void glut_reshape(int width, int height) {
    resize(width, height);
}

void glut_specialfunc(int key, int x, int y) {
    switch ( key ) {
    case GLUT_KEY_UP: {
        if (g_selrowidx + 1 >= rowdata.size())
            break;
        ++g_selrowidx;
        unsigned long long proc;
        unsigned long long thread;
        size_t pos;
        find_task(g_selrowidx, (selectionx0 + selectionx1) / 2.0, proc, thread, pos);
        select_task(proc, thread, pos);
        break;
    }
    case GLUT_KEY_DOWN: {
        if (g_selrowidx == 0)
            break;
        --g_selrowidx;
        unsigned long long proc;
        unsigned long long thread;
        size_t pos;
        find_task(g_selrowidx, (selectionx0 + selectionx1) / 2.0, proc, thread, pos);
        select_task(proc, thread, pos);
        break;
    }
    case GLUT_KEY_LEFT: {
        unsigned long long proc = rowdata[g_selrowidx].first;
        unsigned long long thread = rowdata[g_selrowidx].second;
        if (g_seltask == 0)
            break;
        --g_seltask;
        select_task(proc, thread, g_seltask);
        break;
    }
    case GLUT_KEY_RIGHT: {
        unsigned long long proc = rowdata[g_selrowidx].first;
        unsigned long long thread = rowdata[g_selrowidx].second;
        vector<Entry> &tasks(tasksperproc[proc][thread]);
        if (g_seltask+1 == tasks.size())
            break;
        ++g_seltask;
        select_task(proc, thread, g_seltask);
        break;
    }
    }
}

void glut_keyPressed(unsigned char key, int x, int y) {
    switch ( key ) {
    case '*':
        m_pos[0] = 0.05 * viewx1;
        m_pos[1] = 0.05 * viewy1;
        scalex = 0.9;
        scaley = 0.9;
        paint();
        break;
    case 27:  // escape
        exit(0);
    }
}

void glut_keyUp(unsigned char key, int x, int y) {
}

int glut_btn = 0;
void glut_mouse(int button, int state, int x, int y) {

    if (button == GLUT_LEFT_BUTTON) glut_btn = 1;
    else if (button == GLUT_RIGHT_BUTTON) glut_btn = 2;

    if (state == GLUT_DOWN)
        mouse_click( x, y, glut_btn );
    else if (state == GLUT_UP)
        mouse_release( x, y, glut_btn);
}

void glut_motion(int x, int y) {
    mouse_move( x, y, glut_btn );
    paint();
}

void glut_display() {
    doPaint();
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim))
        elems.push_back(item);
}

bool parse(const char *filename) {
    std::vector<Entry> alltasks;

    std::ifstream infile(filename);
    if (infile.fail()) {
        std::cerr << "Read failed" << std::endl;
        return false;
    }
    std::string line;

    while (!infile.eof()) {
        getline(infile, line);
        if (line.empty())
            break;
        std::istringstream iss(line, istringstream::in);
        if (line == "LOG 2")
            continue;

        size_t pos = line.find(":");
        if (pos == string::npos) {
            std::cerr<<"Parse error [" << line << "]" << std::endl;
            return false;
        }

        Entry e;
        {
            stringstream ss(line.substr(0, pos));
            unsigned long long tmp;

            ss >> tmp;
            if (ss) {
                e.proc = tmp;
                ss >> e.thread;
            }
            else {
                e.proc = 0;
                e.thread = tmp;
            }
        }

        {
            stringstream ss(line.substr(pos+1, string::npos));
            ss >> e.start;
            ss >> e.length;
            string name;
            getline(ss, name);
            e.name = name.substr(1, string::npos);
        }

        alltasks.push_back(e);
    }
    infile.close();

    if (alltasks.empty()) {
        std::cerr << "Empty log" << std::endl;
        return false;
    }

    // store all task names (for coloring later)
    for (size_t i = 0; i < alltasks.size(); ++i) {
        std::string s = alltasks[i].name;
        size_t spos = s.find(' ');
        if (spos != std::string::npos)
            s = s.substr(0, spos);
        if (g_tasknames.find(s) == g_tasknames.end()) {
            g_tasknames[s] = g_tasknames.size();
        }
    }

    // normalize start time
    unsigned long long starttime = alltasks[0].start;
    for (size_t i = 0; i < alltasks.size(); ++i) {
        if (alltasks[i].start < starttime)
            starttime = alltasks[i].start;
    }

    unsigned long long endtime = 0;
    unsigned long long totaltime = 0;
    for (size_t i = 0; i < alltasks.size(); ++i) {
        alltasks[i].start -= starttime;
        if (alltasks[i].start + alltasks[i].length > endtime)
            endtime = alltasks[i].start + alltasks[i].length;
        totaltime += alltasks[i].length;
    }

    cout << "#tasks=" << alltasks.size()
            << " endtime=" << format(endtime)
            << " time=" << format(totaltime)
            << " parallelism=" << std::setprecision(3) << std::fixed
 << totaltime/(double) endtime
            << endl;

    // fix process and thread ids
    set<unsigned long long> procs;
    for (size_t i = 0; i < alltasks.size(); ++i)
        procs.insert(alltasks[i].proc);
    vector<unsigned long long> procvec(procs.begin(), procs.end());
    sort(procvec.begin(), procvec.end());
    map<unsigned long long, unsigned long long> procmap;
    for (size_t i = 0; i < procvec.size(); ++i)
        procmap[ procvec[i] ] = i;

    vector< set<unsigned long long> > threads;
    threads.resize(procs.size());

    for (size_t i = 0; i < alltasks.size(); ++i) {
        alltasks[i].proc = procmap[ alltasks[i].proc ];
        threads[ alltasks[i].proc ].insert( alltasks[i].thread );
    }

    vector< map<unsigned long long, unsigned long long> > threadmap;
    threadmap.resize( threads.size() );

    for (size_t i = 0; i < procvec.size(); ++i) {
        vector<unsigned long long> threadvec(threads[i].begin(), threads[i].end());
        sort(threadvec.begin(), threadvec.end());
        for (size_t j = 0; j < threadvec.size(); ++j)
            threadmap[i][ threadvec[j] ] = j;
    }
    tasksperproc.resize(procvec.size());
    for (size_t i = 0; i < procvec.size(); ++i)
        tasksperproc[i].resize( threadmap[i].size() );
    for (size_t i = 0; i < alltasks.size(); ++i) {
        alltasks[i].thread = threadmap[ alltasks[i].proc ][alltasks[i].thread];
        tasksperproc[ alltasks[i].proc ][ alltasks[i].thread ].push_back( alltasks[i] );
    }

    return true;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [filename]" << std::endl;
        return 0;
    }

    if (!parse(argv[1]))
        return 1;

    std::string title("SuperGlue Visualizator: ");
    title += argv[1];

    setup();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB );
    glutInitWindowPosition(50, 25);
    glutInitWindowSize(1024, 640);
    glutCreateWindow(title.c_str());
    glutReshapeFunc(glut_reshape);
    glutDisplayFunc(glut_display);
    glutMouseFunc(glut_mouse);
    glutMotionFunc(glut_motion);
    glutKeyboardFunc(glut_keyPressed);
    glutKeyboardUpFunc(glut_keyUp);
    glutSpecialFunc(glut_specialfunc);
    glutMainLoop();
    return 0;
}
