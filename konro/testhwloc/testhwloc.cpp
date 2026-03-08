#include <hwloc.h>
#include <iostream>
#include <vector>

using namespace std;

hwloc_topology_t topology;

static void initTopology()
{
    // See: /usr/share/doc/libhwloc-dev/hwloc-hello.c.gz

    // Allocate and initialize topology object
    hwloc_topology_init(&topology);

#if HWLOC_API_VERSION >= 0x00020500
    // For NUMA nodes see:
    // https://www.open-mpi.org/projects/hwloc/doc/v2.3.0/a00360.php
    hwloc_topology_set_all_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
#endif

    // Perform the topology detection
    hwloc_topology_load(topology);
}

/*!
 * \brief Finds an object in the hwloc tree using its OS index
 * \param objType the hwloc type of the object
 * \param osIdx OS index of the object
 * \return object or nullptr
 */
static hwloc_obj_t findObjByOsIndex(hwloc_obj_type_t objType, int osIdx)
{
    hwloc_obj_t obj = nullptr;
    while ((obj = hwloc_get_next_obj_by_type(topology, objType, obj)) != nullptr) {
        if (obj->os_index == osIdx) {
            return obj;
        }
    }
    return nullptr;
}

/*!
 * Dumps the hwloc topolgy by depth
 */
static void testHwloc1()
{
    int topodepth = hwloc_topology_get_depth(topology);
    cout << "Topology depth = " << topodepth << endl;
    for (int depth = 0; depth < topodepth; ++depth) {
        int nobjs = hwloc_get_nbobjs_by_depth(topology, depth);
        cout << "*** Objects at level " << depth << ": " << nobjs << endl;
        for (int i = 0; i < nobjs; ++i) {
            hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, depth, i);
            char objType[64], attr[1024];
            hwloc_obj_type_snprintf(objType, sizeof objType, obj, 0);
            cout << "   *** " << i << ": " << objType;
            hwloc_obj_attr_snprintf(attr, sizeof attr, obj, "/", 0);
            if (*attr) {
                cout << "  (" << attr << ")";
            }
            if (obj->os_index != (unsigned)-1) {
                cout << ", os index: " << obj->os_index;
            }
            cout << endl;
        }
    }
    int num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
    int num_pu = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PU);
    cout << endl << "Cores: " << num_cores << ", processing units: " << num_pu << endl;
}

static void printHwlocObj(int level, void *obj)
{
    if (!obj) {
        return;
    }
    for (int i = 0; i < level; ++i) {
        cout << "    ";       // indent
    }
    hwloc_obj_t o = reinterpret_cast<hwloc_obj_t>(obj);
    char objType[64], attr[1024];
    hwloc_obj_type_snprintf(objType, sizeof objType, o, 0);
    cout << "level " << level << ": object " << objType;
    if (o->os_index != (unsigned)-1) {
        cout << ", os index: " << o->os_index;
    }
    hwloc_obj_attr_snprintf(attr, sizeof attr, o, "/", 0);
    if (*attr) {
        cout << "  (" << attr << ")";
    }
    cout << endl;
    // recursively print the children
    for (int i = 0; i < o->arity; ++i) {
        printHwlocObj(level+1, o->children[i]);
    }
}

/*!
 * Dumps the hwloc tree
 */
static void testHwloc2()
{
    hwloc_obj_t obj = hwloc_get_root_obj(topology);
    printHwlocObj(0, obj);
}

/*!
 * \brief Tests hwloc distance matrix
 */
static void testHwloc3()
{
#if HWLOC_API_VERSION >= 0x00020500
    struct hwloc_distances_s *distances[1];
    unsigned int nr = 1;
    unsigned long kind = 0;     // all kinds

    int rc = hwloc_distances_get(topology, &nr, distances, kind, 0);
    cout << "hwloc_distances_get returned " << rc << endl;
    if (rc == 0) {
        // no error
        cout << "Created " << nr << " matrices" << endl;
    } else {
        cout << "ERROR calling hwloc_distances_get" << endl;
    }
    if (nr > 0) {
        hwloc_distances_release(topology, distances[0]);
    } else {
        cout << "No distance to release" << endl;
    }
#endif
}

int objDistance(hwloc_obj_t o1, hwloc_obj_t o2)
{
    if (o1 == nullptr || o2 == nullptr) {
        return 10;
    }
    int distance = 0;
    while (o1 != o2 && o1 != nullptr && o2 != nullptr) {
        o1 = o1->parent;
        o2 = o2->parent;
        ++distance;
    }
    return distance;
}

int puDistance(int osidx1, int osidx2)
{
    hwloc_obj_t o1 = findObjByOsIndex(HWLOC_OBJ_PU, osidx1);
    hwloc_obj_t o2 = findObjByOsIndex(HWLOC_OBJ_PU, osidx2);
    return objDistance(o1, o2);
}

static void testDistance()
{
    vector<hwloc_obj_t> pu_objs;
    int depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PU);
    int num_pu = hwloc_get_nbobjs_by_depth(topology, depth);
    for (int i = 0; i < num_pu; ++i) {
        hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, depth, i);
        pu_objs.push_back(obj);
    }
    cout << "TEST DISTANCE" << endl;
    cout << "Found " << num_pu << " PU at level " << depth<< endl;
    for (int i = 0; i < num_pu-1; ++i) {
        for (int j = i; j < num_pu; ++j) {
            int distance1 = objDistance(pu_objs[i], pu_objs[j]);
            int distance2 = puDistance(pu_objs[i]->os_index, pu_objs[j]->os_index);
            if (distance1 != distance2) {
                cout << "ERROR\n";
            }
            cout << "Distance "
                 << "pu" << pu_objs[i]->os_index
                 << " : "
                 << "pu" << pu_objs[j]->os_index
                 << " = " << distance1 << endl;
        }
    }
    cout << endl;
}

int main()
{
    initTopology();
    testHwloc1();
    testHwloc2();
    testHwloc3();
    testDistance();
}
