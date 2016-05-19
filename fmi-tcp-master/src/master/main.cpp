#include <string>
#include <fmitcp/Client.h>
#include <fmitcp/common.h>
#include <fmitcp/Logger.h>
#ifdef USE_MPI
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
#include <fstream>

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
static vector<FMIClient*> setupClients(int numFMUs) {
    vector<FMIClient*> clients;
    for (int x = 0; x < numFMUs; x++) {
        FMIClient* client = new FMIClient(x+1 /* world_rank */, x /* clientId */);
        clients.push_back(client);
    }
    return clients;
}
#else
static FMIClient* connectClient(std::string uri, zmq::context_t &context, int clientId){
    struct parsed_url * url = parse_url(uri.c_str());

    if (!url || !url->port || !url->host) {
        parsed_url_free(url);
        return NULL;
    }

    FMIClient* client = new FMIClient(context, clientId, url->host, atoi(url->port));
    parsed_url_free(url);

    return client;
}

static vector<FMIClient*> setupClients(vector<string> fmuURIs, zmq::context_t &context) {
    vector<FMIClient*> clients;
    int clientId = 0;
    for (auto it = fmuURIs.begin(); it != fmuURIs.end(); it++, clientId++) {
        // Assume URI to client
        FMIClient *client = connectClient(*it, context, clientId);

        if (!client) {
            fprintf(stderr, "Failed to connect client with URI %s\n", it->c_str());
            exit(1);
        }

        clients.push_back(client);
    }
    return clients;
}
#endif

static vector<WeakConnection> setupWeakConnections(vector<connection> connections, vector<FMIClient*> clients) {
    vector<WeakConnection> weakConnections;
    for (auto it = connections.begin(); it != connections.end(); it++) {
        weakConnections.push_back(WeakConnection(*it, clients[it->fromFMU], clients[it->toFMU]));
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
        vector<FMIClient*> clients, vector<WeakConnection> &weakConnections, parameter_map &params) {
    clientvarmap maps;
    for (auto client : clients) {
        maps[client] = client->getVariables();
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
            connection conn;
            conn.fromType = conn.toType = fmi2_base_type_real;
            conn.fromOutputVR = ovarit->second.vr;
            conn.toInputVR = varit->second.vr;
            weakConnections.push_back(WeakConnection(conn, ovarmapit->first, varmapit->first));

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

static StrongConnector* findOrCreateBallLockConnector(FMIClient *client,
        int posX, int posY, int posZ,
        int accX, int accY, int accZ,
        int forceX, int forceY, int forceZ,
        int quatX, int quatY, int quatZ, int quatW,
        int angAccX, int angAccY, int angAccZ,
        int torqueX, int torqueY, int torqueZ) {
    for (int x = 0; x < client->numConnectors(); x++) {
        StrongConnector *sc = client->getConnector(x);
        if (sc->matchesBallLockConnector(
                posX, posY, posZ, accX, accY, accZ, forceX, forceY, forceZ,
                quatX, quatY, quatZ, quatW, angAccX, angAccY, angAccZ, torqueX, torqueY, torqueZ)) {
            return sc;
        }
    }
    StrongConnector *sc = client->createConnector();
    sc->setPositionValueRefs           (posX, posY, posZ);
    sc->setAccelerationValueRefs       (accX, accY, accZ);
    sc->setForceValueRefs              (forceX, forceY, forceZ);
    sc->setQuaternionValueRefs         (quatX, quatY, quatZ, quatW);
    sc->setAngularAccelerationValueRefs(angAccX, angAccY, angAccZ);
    sc->setTorqueValueRefs             (torqueX, torqueY, torqueZ);
    return sc;
}

static StrongConnector* findOrCreateShaftConnector(FMIClient *client,
        int angle, int angularVel, int angularAcc, int torque) {
    for (int x = 0; x < client->numConnectors(); x++) {
        StrongConnector *sc = client->getConnector(x);
        if (sc->matchesShaftConnector(angle, angularVel, angularAcc, torque)) {
            fprintf(stderr, "Match! id = %i\n", sc->m_index);
            return sc;
        }
    }
    StrongConnector *sc = client->createConnector();
    sc->setShaftAngleValueRef          (angle);
    sc->setAngularVelocityValueRefs    (angularVel, -1, -1);
    sc->setAngularAccelerationValueRefs(angularAcc, -1, -1);
    sc->setTorqueValueRefs             (torque,     -1, -1);
    return sc;
}

static void setupConstraintsAndSolver(vector<strongconnection> strongConnections, vector<FMIClient*> clients, Solver *solver) {
    for (auto it = strongConnections.begin(); it != strongConnections.end(); it++) {
        //NOTE: this leaks memory, but I don't really care since it's only setup
        Constraint *con;
        char t = tolower(it->type[0]);

        switch (t) {
        case 'b':
        case 'l':
        {
            if (it->vrs.size() != 38) {
                fprintf(stderr, "Bad %s specification: need 38 VRs ([XYZpos + XYZacc + XYZforce + Quat + XYZrotAcc + XYZtorque] x 2), got %zu\n",
                        t == 'b' ? "ball joint" : "lock", it->vrs.size());
                exit(1);
            }

            StrongConnector *scA = findOrCreateBallLockConnector(clients[it->fromFMU],
                                                 it->vrs[0], it->vrs[1], it->vrs[2],
                                                 it->vrs[3], it->vrs[4], it->vrs[5],
                                                 it->vrs[6], it->vrs[7], it->vrs[8],
                                                 it->vrs[9], it->vrs[10],it->vrs[11],it->vrs[12],
                                                 it->vrs[13],it->vrs[14],it->vrs[15],
                                                 it->vrs[16],it->vrs[17],it->vrs[18]);

            StrongConnector *scB = findOrCreateBallLockConnector(clients[it->toFMU],
                                                 it->vrs[19],it->vrs[20],it->vrs[21],
                                                 it->vrs[22],it->vrs[23],it->vrs[24],
                                                 it->vrs[25],it->vrs[26],it->vrs[27],
                                                 it->vrs[28],it->vrs[29],it->vrs[30],it->vrs[31],
                                                 it->vrs[32],it->vrs[33],it->vrs[34],
                                                 it->vrs[35],it->vrs[36],it->vrs[37]);

            con = t == 'b' ? new BallJointConstraint(scA, scB, Vec3(), Vec3())
                           : new LockConstraint(scA, scB, Vec3(), Vec3(), Quat(), Quat());

            break;
        }
        case 's':
        {
            int ofs = 0;
            if (it->vrs.size() != 8) {
                //maybe it's the old type of specification?
                ofs = 1;
                if (it->vrs.size() != 9) {
                    fprintf(stderr, "Bad shaft specification: need 8 VRs ([shaft angle + angular velocity + angular acceleration + torque] x 2)\n");
                    exit(1);
                }
            }

            StrongConnector *scA = findOrCreateShaftConnector(clients[it->fromFMU],
                    it->vrs[ofs+0], it->vrs[ofs+1], it->vrs[ofs+2], it->vrs[ofs+3]);

            StrongConnector *scB = findOrCreateShaftConnector(clients[it->toFMU],
                    it->vrs[ofs+4], it->vrs[ofs+5], it->vrs[ofs+6], it->vrs[ofs+7]);

            con = new ShaftConstraint(scA, scB);
            break;
        }
        default:
            fprintf(stderr, "Unknown strong connector type: %s\n", it->type.c_str());
            exit(1);
        }

        solver->addConstraint(con);
    }

    for (auto it = clients.begin(); it != clients.end(); it++) {
        solver->addSlave(*it);
   }
}

static void sendUserParams(BaseMaster *master, vector<FMIClient*> clients, map<pair<int,fmi2_base_type_enu_t>, vector<param> > params) {
    for (auto it = params.begin(); it != params.end(); it++) {
        FMIClient *client = clients[it->first.first];
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

static string getFieldnames(vector<FMIClient*> clients) {
    ostringstream oss;
    oss << "t";
    for (auto client : clients) {
        ostringstream prefix;
        prefix << " " << "fmu" << client->getId() << "_";
        oss << client->getSpaceSeparatedFieldNames(prefix.str());
    }
    return oss.str();
}

int main(int argc, char *argv[] ) {
#ifdef USE_MPI
    fprintf(stderr, "MPI enabled\n");
    MPI_Init(NULL, NULL);
#else
    fprintf(stderr, "MPI disabled\n");
#endif

    fmitcp::Logger logger;
    double timeStep = 0.1;
    double startTime = 0;
    double endTime = 10;
    double relativeTolerance = 0.0001;
    double relaxation = 4,
           compliance = 0;
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
    string fieldnameFilename;
    bool holonomic = true;

    if (parseArguments(
            argc, argv, &fmuURIs, &connections, &params, &endTime, &timeStep,
            &loggingOn, &csv_separator, &outFilePath, &quietMode, &fileFormat,
            &method, &realtimeMode, &printXML, &stepOrder, &fmuVisibilities,
            &scs, &connconf, &hdf5Filename, &fieldnameFilename, &holonomic, &compliance)) {
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

#ifdef USE_MPI
    //world = master at 0, FMUs at 1..N
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank != 0) {
        fprintf(stderr, "fmi-mpi-master: Expected world_rank = 0, got %i\n", world_rank);
        return 1;
    }

    vector<FMIClient*> clients = setupClients(world_size-1);
#else
    zmq::context_t context(1);
    //without this the maximum number of clients tops out at 300 on Linux,
    //around 63 on Windows (according to Web searches)
#ifdef ZMQ_MAX_SOCKETS
    zmq_ctx_set((void *)context, ZMQ_MAX_SOCKETS, fmuURIs.size());
#endif
    vector<FMIClient*> clients = setupClients(fmuURIs, context);
#endif

    //connect, get modelDescription XML (important for connconf)
    for (auto it = clients.begin(); it != clients.end(); it++) {
        (*it)->connect();
    }

    vector<WeakConnection> weakConnections = setupWeakConnections(connections, clients);
    addAutomaticConnectionsAndParams(connconf, clients, weakConnections, params);
    setupConstraintsAndSolver(scs, clients, &solver);

    BaseMaster *master;
    string fieldnames = getFieldnames(clients);

    if (scs.size()) {
        if (method != jacobi) {
            fprintf(stderr, "Can only do Jacobi stepping for weak connections when also doing strong coupling\n");
            return 1;
        }

        solver.setSpookParams(relaxation,compliance,timeStep);
        StrongMaster *sm = new StrongMaster(clients, weakConnections, solver, holonomic);
        master = sm;
        fieldnames += sm->getForceFieldnames();
    } else {
        master = (method == gs) ?           (BaseMaster*)new GaussSeidelMaster(clients, weakConnections, stepOrder) :
                                            (BaseMaster*)new JacobiMaster(clients, weakConnections);
    }

    if (fieldnameFilename.length() > 0) {
        ofstream ofs(fieldnameFilename.c_str());
        ofs << fieldnames;
        ofs << endl;
    }

    //hook clients to master
    for (auto it = clients.begin(); it != clients.end(); it++) {
        (*it)->m_master = master;
    }

    //init
    for (size_t x = 0; x < clients.size(); x++) {
        //set visibility based on command line
        master->send(clients[x], fmi2_import_instantiate2(0, x < fmuVisibilities.size() ? fmuVisibilities[x] : false));
    }

    //TODO: send initial values parsed from XML

    //send user-defined parameters
    sendUserParams(master, clients, params);

    master->block(clients, fmi2_import_initialize_slave(0, 0, true, relativeTolerance, startTime, endTime >= 0, endTime));

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
        for (auto it = clients.begin(); it != clients.end(); it++) {
            master->send(*it, fmi2_import_get_real(0, 0, (*it)->getRealOutputValueReferences()));
        }
        
        PRINT_HDF5_DELTA("get_outputs");

        master->wait();
        
        PRINT_HDF5_DELTA("get_outputs_wait");

        //print as CSV
        printf("%f", t);
        for (auto it = clients.begin(); it != clients.end(); it++) {
            for (auto it2 = (*it)->m_getRealValues.begin(); it2 != (*it)->m_getRealValues.end(); it2++) {
                printf("%c%f", csv_separator, *it2);
            }
        }

        PRINT_HDF5_DELTA("print_csv");

        master->runIteration(t, timeStep);
        
        printf("\n");
        t += timeStep;

#ifndef WIN32
        //HDF5
        nrecords++;
#endif
    }

#ifndef WIN32
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
#endif

    fprintf(stderr, "\n");

    //clean up
    delete master;

    for (size_t x = 0; x < clients.size(); x++) {
        delete clients[x];
    }

#ifdef USE_MPI
    //send shutdown message (tag = 1), finalize
    for (int x = 1; x < world_size; x++) {
        MPI_Send(NULL, 0, MPI_CHAR, x, 1, MPI_COMM_WORLD);
    }
    MPI_Finalize();
#endif

    return 0;
}
