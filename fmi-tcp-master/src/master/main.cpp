#include <string>
#include <fmitcp/Client.h>
#include <fmitcp/common.h>
#include <fmitcp/Logger.h>
#ifdef USE_LACEWING
#include <fmitcp/EventPump.h>
#elif defined(USE_MPI)
#include <mpi.h>
#else
#include <zmq.hpp>
#endif
#include <fmitcp/serialize.h>
#include <stdlib.h>
#include <signal.h>
#include <sstream>
#include "master/FMIClient.h"
#include "common/common.h"
#include "master/WeakMasters.h"
#include "common/url_parser.h"
#include "master/parseargs.h"
#include <sc/BallJointConstraint.h>
#include <sc/LockConstraint.h>
#include <sc/ShaftConstraint.h>
#include "master/StrongMaster.h"
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

using namespace fmitcp_master;
using namespace fmitcp;
using namespace fmitcp::serialize;
using namespace sc;

#ifndef WIN32
timeval tl1, tl2;
vector<int> timelog;
int columnofs;
std::map<int, const char*> columnnames;
#endif

typedef map<pair<int,fmi2_base_type_enu_t>, vector<param> > parameter_map;

#ifdef USE_MPI
static vector<FMIClient*> setupSlaves(int numFMUs) {
    vector<FMIClient*> slaves;
    for (int x = 0; x < numFMUs; x++) {
        FMIClient* client = new FMIClient(x+1 /* world_rank */, x /* slaveId */);
        slaves.push_back(client);
    }
    return slaves;
}
#else
#ifdef USE_LACEWING
static FMIClient* connectSlave(std::string uri, fmitcp::EventPump *pump, int slaveId){
#else
    static FMIClient* connectSlave(std::string uri, zmq::context_t &context, int slaveId){
#endif
    struct parsed_url * url = parse_url(uri.c_str());

    if (!url || !url->port || !url->host) {
        parsed_url_free(url);
        return NULL;
    }

#ifdef USE_LACEWING
    FMIClient* client = new FMIClient(pump, slaveId, url->host, atoi(url->port));
#else
    FMIClient* client = new FMIClient(context, slaveId, url->host, atoi(url->port));
#endif
    parsed_url_free(url);

    return client;
}

#ifdef USE_LACEWING
static vector<FMIClient*> setupSlaves(vector<string> fmuURIs, EventPump *pump) {
#else
static vector<FMIClient*> setupSlaves(vector<string> fmuURIs, zmq::context_t &context) {
#endif
    vector<FMIClient*> slaves;
    int slaveId = 0;
    for (auto it = fmuURIs.begin(); it != fmuURIs.end(); it++, slaveId++) {
        // Assume URI to slave
#ifdef USE_LACEWING
        FMIClient *slave = connectSlave(*it, pump, slaveId);
#else
        FMIClient *slave = connectSlave(*it, context, slaveId);
#endif

        if (!slave) {
            fprintf(stderr, "Failed to connect slave with URI %s\n", it->c_str());
            exit(1);
        }

        slaves.push_back(slave);
    }
    return slaves;
}
#endif

static vector<WeakConnection*> setupWeakConnections(vector<connection> connections, vector<FMIClient*> slaves) {
    vector<WeakConnection*> weakConnections;
    for (auto it = connections.begin(); it != connections.end(); it++) {
        if (it->type == fmi2_base_type_real) {
            fprintf(stderr, "Creating weak connection FMU %d VR %d -> FMU %d VR %d\n", it->fromFMU, it->fromOutputVR, it->toFMU, it->toInputVR);
            weakConnections.push_back(new WeakConnection(slaves[it->fromFMU], slaves[it->toFMU], it->fromOutputVR, it->toInputVR));
        } else {
            fprintf(stderr, "Unsupported connection type %i\n", it->type);
            exit(1);
        }
    }
    return weakConnections;
}

/**
 * Look for exactly zero or one match for given node+signal name and given causality
 */
typedef map<FMIClient*, variable_map> clientvarmap;
static bool matchNodesignal(const clientvarmap& varmaps, fmi2_causality_enu_t causality,
        nodesignal ns, clientvarmap::const_iterator& varmapit, variable_map::const_iterator& varit) {
    //fprintf(stderr, "looking for signal %s, causality %i\n", ns.signal.c_str(), causality);
    varmapit = varmaps.end();
    size_t nmatches = 0;
    for (clientvarmap::const_iterator m = varmaps.begin(); m != varmaps.end(); m++) {
        varit = m->second.find(ns.signal);
        if (m->first->getModelName() == ns.node && varit != m->second.end() && varit->second.causality == causality) {
            //found something!
            varmapit = m;
            nmatches++;
        }
    }
    if (nmatches > 1) {
        fprintf(stderr, "Found %li matches for node \"%s\" signal \"%s\" (expected zero or one) - bailing out!\n", nmatches, ns.node.c_str(), ns.signal.c_str());
        exit(1);
    }
    return nmatches == 1;
}

static void addAutomaticConnectionsAndParams(const vector<connectionconfig> &connconf,
        vector<FMIClient*> slaves, vector<WeakConnection*> &weakConnections, parameter_map &params) {
    clientvarmap maps;
    for (auto slave : slaves) {
        maps[slave] = slave->getVariables();
    }
    for (auto cc : connconf) {
        //first see if we can find a corresponding input
        clientvarmap::const_iterator varmapit, ovarmapit;
        variable_map::const_iterator varit, ovarit;
        bool match = matchNodesignal(maps, fmi2_causality_enu_input, cc.input, varmapit, varit);

        //fprintf(stderr, "signal %s matched? %i\n", cc.input.signal.c_str(), match);
        if (!match) {
            //this happens often, nothing to alarm the user about
            continue;
        }

        match = false;
        nodesignal ons;
        //fprintf(stderr, "%li outputs\n", cc.outputs.size());
        for (auto o : cc.outputs) {
            if ((match = matchNodesignal(maps, fmi2_causality_enu_output, o, ovarmapit, ovarit))) {
                ons = o;
                break;
            }
        }

        if (match) {
            fprintf(stderr, "Creating weak connection FMU %d VR %d -> FMU %d VR %d [automatic] (node \"%s\" signal \"%s\" -> node \"%s\" signal \"%s\")\n",
                    ovarmapit->first->getId(), ovarit->second.vr, varmapit->first->getId(), varit->second.vr,
                    ons.node.c_str(), ons.signal.c_str(), cc.input.node.c_str(), cc.input.signal.c_str());
            weakConnections.push_back(new WeakConnection(ovarmapit->first, varmapit->first, ovarit->second.vr, varit->second.vr));

            if (varit->second.type != fmi2_base_type_real) {
                fprintf(stderr, "Only real valued weak connections supported at the moment\n");
                exit(1);
            }
        } else {
            //no match - see if there's a default value
            if (cc.hasDefault) {
                //TODO: actually print the value? a bit of a hassle right not since params have variable type
                fprintf(stderr, "Default value -> FMU %d VR %d [automatic] (node \"%s\" signal \"%s\")\n",
                    varmapit->first->getId(), varit->second.vr, cc.input.node.c_str(), cc.input.signal.c_str());
                param p = cc.defaultValue;
                p.fmuIndex = varmapit->first->getId();
                p.valueReference = varit->second.vr;
                params[make_pair(p.fmuIndex, varit->second.type)].push_back(p);
            } else {
                //no default value - fail!
                fprintf(stderr, "Node \"%s\" signal \"%s\" has input but no output or default value!\n",
                        cc.input.node.c_str(), cc.input.signal.c_str());
                exit(1);
            }
        }
    }
}

static void setupConstraintsAndSolver(vector<strongconnection> strongConnections, vector<FMIClient*> slaves, Solver *solver) {
    for (auto it = strongConnections.begin(); it != strongConnections.end(); it++) {
        //NOTE: this leaks memory, but I don't really care since it's only setup
        StrongConnector *scA = slaves[it->fromFMU]->createConnector();
        StrongConnector *scB = slaves[it->toFMU]->createConnector();
        Constraint *con;
        char t = tolower(it->type[0]);

        switch (t) {
        case 'b':
        case 'l':
            if (it->vrs.size() != 38) {
                fprintf(stderr, "Bad %s specification: need 38 VRs ([XYZpos + XYZacc + XYZforce + Quat + XYZrotAcc + XYZtorque] x 2), got %zu\n",
                        t == 'b' ? "ball joint" : "lock", it->vrs.size());
                exit(1);
            }

            scA->setPositionValueRefs           (it->vrs[0], it->vrs[1], it->vrs[2]);
            scA->setAccelerationValueRefs       (it->vrs[3], it->vrs[4], it->vrs[5]);
            scA->setForceValueRefs              (it->vrs[6], it->vrs[7], it->vrs[8]);
            scA->setQuaternionValueRefs         (it->vrs[9], it->vrs[10],it->vrs[11],it->vrs[12]);
            scA->setAngularAccelerationValueRefs(it->vrs[13],it->vrs[14],it->vrs[15]);
            scA->setTorqueValueRefs             (it->vrs[16],it->vrs[17],it->vrs[18]);

            scB->setPositionValueRefs           (it->vrs[19],it->vrs[20],it->vrs[21]);
            scB->setAccelerationValueRefs       (it->vrs[22],it->vrs[23],it->vrs[24]);
            scB->setForceValueRefs              (it->vrs[25],it->vrs[26],it->vrs[27]);
            scB->setQuaternionValueRefs         (it->vrs[28],it->vrs[29],it->vrs[30],it->vrs[31]);
            scB->setAngularAccelerationValueRefs(it->vrs[32],it->vrs[33],it->vrs[34]);
            scB->setTorqueValueRefs             (it->vrs[35],it->vrs[36],it->vrs[37]);

            con = t == 'b' ? new BallJointConstraint(scA, scB, Vec3(), Vec3())
                           : new LockConstraint(scA, scB, Vec3(), Vec3(), Quat(), Quat());

            break;
        case 's':
        {
            if (it->vrs.size() != 9) {
                fprintf(stderr, "Bad shaft specification: need axis (0 = X, 1 = Y, 2 = Z) and 8 VRs ([shaft angle + angular velocity + angular acceleration + torque] x 2)\n");
                exit(1);
            }

            int axis = it->vrs[0];

            if (axis < 0 || axis > 2) {
                fprintf(stderr, "Bad axis: %i\n", axis);
                exit(1);
            }

            scA->setShaftAngleValueRef(it->vrs[1]);
            scA->setAngularVelocityValueRefs    (axis == 0 ? it->vrs[2] : -1, axis == 1 ? it->vrs[2] : -1, axis == 2 ? it->vrs[2] : -1);
            scA->setAngularAccelerationValueRefs(axis == 0 ? it->vrs[3] : -1, axis == 1 ? it->vrs[3] : -1, axis == 2 ? it->vrs[3] : -1);
            scA->setTorqueValueRefs             (axis == 0 ? it->vrs[4] : -1, axis == 1 ? it->vrs[4] : -1, axis == 2 ? it->vrs[4] : -1);

            scB->setShaftAngleValueRef(it->vrs[5]);
            scB->setAngularVelocityValueRefs    (axis == 0 ? it->vrs[6] : -1, axis == 1 ? it->vrs[6] : -1, axis == 2 ? it->vrs[6] : -1);
            scB->setAngularAccelerationValueRefs(axis == 0 ? it->vrs[7] : -1, axis == 1 ? it->vrs[7] : -1, axis == 2 ? it->vrs[7] : -1);
            scB->setTorqueValueRefs             (axis == 0 ? it->vrs[8] : -1, axis == 1 ? it->vrs[8] : -1, axis == 2 ? it->vrs[8] : -1);

            con = new ShaftConstraint(scA, scB, axis);
            break;
        }
        default:
            fprintf(stderr, "Unknown strong connector type: %s\n", it->type.c_str());
            exit(1);
        }

        solver->addConstraint(con);
    }

    for (auto it = slaves.begin(); it != slaves.end(); it++) {
        solver->addSlave(*it);
   }
}

static void cleanUp(BaseMaster *master, vector<FMIClient*> slaves, vector<WeakConnection*> weakConnections) {
    //clean up
    delete master;

    for (size_t x = 0; x < slaves.size(); x++) {
        delete slaves[x];
    }

    for (size_t x = 0; x < weakConnections.size(); x++) {
        delete weakConnections[x];
    }
}

static void sendUserParams(BaseMaster *master, vector<FMIClient*> slaves, map<pair<int,fmi2_base_type_enu_t>, vector<param> > params) {
    for (auto it = params.begin(); it != params.end(); it++) {
        FMIClient *client = slaves[it->first.first];
        vector<int> vrs;
        for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            vrs.push_back(it2->valueReference);
        }

        switch (it->first.second) {
        case fmi2_base_type_real: {
            vector<double> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->realValue);
            }
            master->send(client, fmi2_import_set_real(0, 0, vrs, values));
            break;
        }
        case fmi2_base_type_enum:
        case fmi2_base_type_int: {
            vector<int> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->intValue);
            }
            master->send(client, fmi2_import_set_integer(0, 0, vrs, values));
            break;
        }
        case fmi2_base_type_bool: {
            vector<bool> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->boolValue);
            }
            master->send(client, fmi2_import_set_boolean(0, 0, vrs, values));
            break;
        }
        case fmi2_base_type_str: {
            vector<string> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->stringValue);
            }
            master->send(client, fmi2_import_set_string(0, 0, vrs, values));
            break;
        }
        }
    }
}

int main(int argc, char *argv[] ) {
#ifdef USE_MPI
    fprintf(stderr, "MPI enabled\n");
#else
    fprintf(stderr, "MPI disabled\n");
#endif

    fmitcp::Logger logger;
#ifdef USE_LACEWING
    fmitcp::EventPump pump;
#endif
    double timeStep = 0.1;
    double startTime = 0;
    double endTime = 10;
    double relativeTolerance = 0.0001;
    double relaxation = 12,
           compliance = 0.01;
    vector<string> fmuURIs;
    vector<connection> connections;
    parameter_map params;
    int loggingOn = 0;
    char csv_separator = ',';
    string outFilePath = DEFAULT_OUTFILE;
    int quietMode = 0;
    FILEFORMAT fileFormat = csv;
    METHOD method = jacobi;
    int realtimeMode = 0;
    int printXML = 0;
    vector<int> stepOrder;
    vector<int> fmuVisibilities;
    vector<strongconnection> scs;
    vector<connectionconfig> connconf;
    Solver solver;
    string hdf5Filename;

    if (parseArguments(
            argc, argv, &fmuURIs, &connections, &params, &endTime, &timeStep,
            &loggingOn, &csv_separator, &outFilePath, &quietMode, &fileFormat,
            &method, &realtimeMode, &printXML, &stepOrder, &fmuVisibilities,
            &scs, &connconf, &hdf5Filename)) {
        return 1;
    }

    if (printXML) {
        fprintf(stderr, "XML mode not implemented\n");
        return 1;
    }

    if (loggingOn) {
        fprintf(stderr, "WARNING: -l not implemented\n");
    }

    if (quietMode) {
        fprintf(stderr, "WARNING: -q not implemented\n");
    }

    if (outFilePath != DEFAULT_OUTFILE) {
        fprintf(stderr, "WARNING: -o not implemented (output always goes to stdout)\n");
    }

#ifdef USE_LACEWING
    vector<FMIClient*> slaves = setupSlaves(fmuURIs, &pump);
#elif defined(USE_MPI)
    //world = master at 0, FMUs at 1..N
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    vector<FMIClient*> slaves = setupSlaves(world_size-1);
#else
    zmq::context_t context(1);
    //without this the maximum number of slaves tops out at 300 on Linux,
    //around 63 on Windows (according to Web searches)
    zmq_ctx_set(context, ZMQ_MAX_SOCKETS, fmuURIs.size());
    vector<FMIClient*> slaves = setupSlaves(fmuURIs, context);
#endif

    //connect, get modelDescription XML (important for connconf)
    for (auto it = slaves.begin(); it != slaves.end(); it++) {
        (*it)->connect();
    }

    vector<WeakConnection*> weakConnections = setupWeakConnections(connections, slaves);
    addAutomaticConnectionsAndParams(connconf, slaves, weakConnections, params);
    setupConstraintsAndSolver(scs, slaves, &solver);

    BaseMaster *master;

    if (scs.size()) {
        if (method != jacobi) {
            fprintf(stderr, "Can only do Jacobi stepping for weak connections when also doing strong coupling\n");
            return 1;
        }

        solver.setSpookParams(relaxation,compliance,timeStep);
#ifdef USE_LACEWING
        master = new StrongMaster(&pump, slaves, weakConnections, solver);
#else
        master = new StrongMaster(slaves, weakConnections, solver);
#endif
    } else {
#ifdef USE_LACEWING
        master = (method == gs) ?           (BaseMaster*)new GaussSeidelMaster(&pump, slaves, weakConnections, stepOrder) :
                                            (BaseMaster*)new JacobiMaster(&pump, slaves, weakConnections);
#else
        master = (method == gs) ?           (BaseMaster*)new GaussSeidelMaster(slaves, weakConnections, stepOrder) :
                                            (BaseMaster*)new JacobiMaster(slaves, weakConnections);
#endif
    }

    //hook clients to master
    for (auto it = slaves.begin(); it != slaves.end(); it++) {
        (*it)->m_master = master;
    }

    //init
    for (size_t x = 0; x < slaves.size(); x++) {
        //set visibility based on command line
        master->send(slaves[x], fmi2_import_instantiate2(0, x < fmuVisibilities.size() ? fmuVisibilities[x] : false));
    }

    //TODO: send initial values parsed from XML

    //send user-defined parameters
    sendUserParams(master, slaves, params);

    master->block(slaves, fmi2_import_initialize_slave(0, 0, true, relativeTolerance, startTime, endTime >= 0, endTime));

    double t = startTime;
#ifdef WIN32
    LARGE_INTEGER freq, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);
#else
    timeval t1;
    gettimeofday(&t1, NULL);
#endif

    //prepare solver and all that
    master->prepare();

#ifndef WIN32
    //HDF5
    int expected_records = 1+1.01*(endTime-startTime)/timeStep, nrecords = 0;
    timelog.reserve(expected_records*MAX_TIME_COLS);

    gettimeofday(&tl1, NULL);
#endif

    //run
    while (endTime < 0 || t < endTime) {
#ifndef WIN32
        //HDF5
        columnofs = 0;
#endif
        fprintf(stderr, "\r                                                   \r");
        if (realtimeMode) {
            double t_wall;

            //delay loop
            for (;;) {
#ifdef WIN32
                LARGE_INTEGER t2;
                QueryPerformanceCounter(&t2);
                t_wall = (t2.QuadPart - t1.QuadPart) / (double)freq.QuadPart;

                if (t_wall >= t)
                    break;

                Yield();
#else
                timeval t2;
                gettimeofday(&t2, NULL);

                t_wall = ((double)t2.tv_sec - t1.tv_sec) + 1.0e-6 * ((double)t2.tv_usec - t1.tv_usec);
                int us = 1000000 * (t - t_wall);

                if (us <= 0)
                    break;

                usleep(us);
#endif
            }

            if (t_wall > t + 1) {
                //print slowdown factor if we start running behind wall clock
                fprintf(stderr, "t=%.3f, t_wall = %.3f (slowdown = realtime / %.2f)", t, t_wall, t_wall / t);
            } else {
                fprintf(stderr, "t=%.3f, t_wall = %.3f", t, t_wall);
            }
        } else {
            fprintf(stderr, "t=%.3f", t);
        }

        //get outputs
        for (auto it = slaves.begin(); it != slaves.end(); it++) {
            master->send(*it, fmi2_import_get_real(0, 0, (*it)->getRealOutputValueReferences()));
        }
        
        PRINT_HDF5_DELTA("get_outputs");

        master->wait();
        
        PRINT_HDF5_DELTA("get_outputs_wait");

        //print as CSV
        printf("%f", t);
        for (auto it = slaves.begin(); it != slaves.end(); it++) {
            for (auto it2 = (*it)->m_getRealValues.begin(); it2 != (*it)->m_getRealValues.end(); it2++) {
                printf("%c%f", csv_separator, *it2);
            }
        }

#ifdef ENABLE_DEMO_HACKS
        //TESTING: send params every frame
        sendUserParams(master, slaves, params);
#endif
        
        PRINT_HDF5_DELTA("print_csv");

        master->runIteration(t, timeStep);
        
        printf("\n");
        t += timeStep;

#ifndef WIN32
        //HDF5
        nrecords++;
#endif
    }

    vector<size_t> field_offset;
    vector<hid_t> field_types;
    vector<const char*> field_names;

    for (size_t x = 0; x < columnnames.size(); x++) {
        field_offset.push_back(x*sizeof(int));
        field_types.push_back(H5T_NATIVE_INT);
        field_names.push_back(columnnames[x]);
    }

    writeHDF5File(hdf5Filename, field_offset, field_types, field_names,
        "Timings", "table", nrecords, columnnames.size()*sizeof(int), &timelog[0]);

    fprintf(stderr, "\n");

    cleanUp(master, slaves, weakConnections);

    return 0;
}
