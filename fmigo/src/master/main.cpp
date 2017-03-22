#include <string>
#include <fmitcp/Client.h>
#include <fmitcp/common.h>
#include <fmitcp/Logger.h>
#ifdef USE_MPI
#include <mpi.h>
#include "common/mpi_tools.h"
#include "server/FMIServer.h"
#endif
#include <zmq.hpp>
#include <fmitcp/serialize.h>
#include <stdlib.h>
#include <signal.h>
#include <sstream>
#include "master/FMIClient.h"
#include "common/common.h"
#include "master/WeakMasters.h"
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
#include "control.pb.h"

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
static vector<FMIClient*> setupClients(vector<string> fmuURIs, zmq::context_t &context) {
    vector<FMIClient*> clients;
    int clientId = 0;
    for (auto it = fmuURIs.begin(); it != fmuURIs.end(); it++, clientId++) {
        // Assume URI to client
        FMIClient *client = new FMIClient(context, clientId, *it);

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

static int vrFromKeyName(FMIClient* client, string key){

  if(isNumeric(key))
    return atoi(key.c_str());

  const variable_map& vars = client->getVariables();

  switch (vars.count(key)){
  case 0:{
    fprintf(stderr,"Error: client(%d):%s\n", client->getId(), key.c_str());
    exit(1);
  }
  case 1:  return vars.find(key)->second.vr;
  default:{
    fprintf(stderr,"Error: Not uniq - client(%d):%s\n", client->getId(), key.c_str());
    exit(1);
  }
  }
}

#define toVR(type, index)                                   \
  vrFromKeyName(clients[it->type##FMU],it->vrORname[index])

static void setupConstraintsAndSolver(vector<strongconnection> strongConnections, vector<FMIClient*> clients, Solver *solver) {
    for (auto it = strongConnections.begin(); it != strongConnections.end(); it++) {
        //NOTE: this leaks memory, but I don't really care since it's only setup
        Constraint *con;
        char t = tolower(it->type[0]);

        switch (t) {
        case 'b':
        case 'l':
        {
            if (it->vrORname.size() != 38) {
                fprintf(stderr, "Bad %s specification: need 38 VRs ([XYZpos + XYZacc + XYZforce + Quat + XYZrotAcc + XYZtorque] x 2), got %zu\n",
                        t == 'b' ? "ball joint" : "lock", it->vrORname.size());
                exit(1);
            }

            StrongConnector *scA = findOrCreateBallLockConnector(clients[it->fromFMU],
                                       toVR(from,0), toVR(from,1), toVR(from,2),
                                       toVR(from,3), toVR(from,4), toVR(from,5),
                                       toVR(from,6), toVR(from,7), toVR(from,8),
                                       toVR(from,9), toVR(from,10), toVR(from,11), toVR(from,12),
                                       toVR(from,13), toVR(from,14), toVR(from,15),
                                       toVR(from,16), toVR(from,17), toVR(from,18) );

            StrongConnector *scB = findOrCreateBallLockConnector(clients[it->toFMU],
                                       toVR(to,19), toVR(to,20), toVR(to,21),
                                       toVR(to,22), toVR(to,23), toVR(to,24),
                                       toVR(to,25), toVR(to,26), toVR(to,27),
                                       toVR(to,28), toVR(to,29), toVR(to,30), toVR(to,31),
                                       toVR(to,32), toVR(to,33), toVR(to,34),
                                       toVR(to,35), toVR(to,36), toVR(to,37) );

            con = t == 'b' ? new BallJointConstraint(scA, scB, Vec3(), Vec3())
                           : new LockConstraint(scA, scB, Vec3(), Vec3(), Quat(), Quat());

            break;
        }
        case 's':
        {
            if (it->vrORname.size() != 8) {
                fprintf(stderr, "Bad shaft specification: need 8 VRs ([shaft angle + angular velocity + angular acceleration + torque] x 2)\n");
                exit(1);
            }

            StrongConnector *scA = findOrCreateShaftConnector(clients[it->fromFMU],
                                       toVR(from,0), toVR(from,1), toVR(from,2), toVR(from,3));

            StrongConnector *scB = findOrCreateShaftConnector(clients[it->toFMU],
                                       toVR(to,4), toVR(to,5), toVR(to,6), toVR(to,7));

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
          vrs.push_back(vrFromKeyName(client, it2->vrORname));
        }

        switch (it->first.second) {
        case fmi2_base_type_real: {
            vector<double> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->realValue);
            }
            master->send(client, fmi2_import_set_real(vrs, values));
            break;
        }
        case fmi2_base_type_enum:
        case fmi2_base_type_int: {
            vector<int> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->intValue);
            }
            master->send(client, fmi2_import_set_integer(vrs, values));
            break;
        }
        case fmi2_base_type_bool: {
            vector<bool> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->boolValue);
            }
            master->send(client, fmi2_import_set_boolean(vrs, values));
            break;
        }
        case fmi2_base_type_str: {
            vector<string> values;
            for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
                values.push_back(it2->stringValue);
            }
            master->send(client, fmi2_import_set_string(vrs, values));
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

static void handleZmqControl(zmq::socket_t& rep_socket, bool *paused, bool *running) {
    zmq::message_t msg;

    //paused means we do blocking polls to avoid wasting CPU time
    while (rep_socket.recv(&msg, (*paused) ? 0 : ZMQ_NOBLOCK) && *running) {
        //got something - make sure it's a control_message with correct
        //version and command set
        control_proto::control_message ctrl;

        if (    ctrl.ParseFromArray(msg.data(), msg.size()) &&
                ctrl.has_version() &&
                ctrl.version() == 1 &&
                ctrl.has_command()) {
            switch (ctrl.command()) {
            case control_proto::control_message::command_pause:
                *paused = true;
                break;
            case control_proto::control_message::command_unpause:
                *paused = false;
                break;
            case control_proto::control_message::command_stop:
                *running = false;
                break;
            case control_proto::control_message::command_state:
                break;
            }

            //always reply with state
            control_proto::state_message state;
            state.set_version(1);

            if (!*running) {
                state.set_state(control_proto::state_message::state_exiting);
            } else if (*paused) {
                state.set_state(control_proto::state_message::state_paused);
            } else {
                state.set_state(control_proto::state_message::state_running);
            }

            string str = state.SerializeAsString();
            zmq::message_t rep(str.length());
            memcpy(rep.data(), str.data(), str.length());
            rep_socket.send(rep);
        }
    }
}

template<typename RFType, typename From> void addVectorToRepeatedField(RFType* rf, const From& from) {
    for (auto f : from) {
        *rf->Add() = f;
    }
}

static void printOutputs(double t, BaseMaster *master, vector<FMIClient*>& clients) {
    vector<vector<variable> > clientOutputs;

    for (auto client : clients) {
        vector<variable> vars = client->getOutputs();
        SendGetXType getX;

        for (auto var : vars) {
            getX[var.type].push_back(var.vr);
        }

        client->sendGetX(getX);
        clientOutputs.push_back(vars);
    }

    master->wait();

    printf("%f", t);
    for (size_t x = 0; x < clients.size(); x++) {
        FMIClient *client = clients[x];
        for (auto out : clientOutputs[x]) {
            switch (out.type) {
            case fmi2_base_type_real:
                printf(",%f", client->m_getRealValues.front());
                client->m_getRealValues.pop_front();
                break;
            case fmi2_base_type_int:
                printf(",%i", client->m_getIntegerValues.front());
                client->m_getIntegerValues.pop_front();
                break;
            case fmi2_base_type_bool:
                printf(",%i", client->m_getBooleanValues.front());
                client->m_getBooleanValues.pop_front();
                break;
            case fmi2_base_type_str: {
                ostringstream oss;
                for(char c: client->m_getStringValues.front()){
                    switch (c){
                    case '"': oss << "\"\""; break;
                    default: oss << c;
                    }
                }
                printf(",\"%s\"", oss.str().c_str());
                client->m_getStringValues.pop_front();
                break;
            }
            case fmi2_base_type_enum:
                fprintf(stderr, "Enum outputs not allowed for now\n");
                exit(1);
            }
        }
    }
}


static void pushResults(int step, double t, double endTime, double timeStep, zmq::socket_t& push_socket, BaseMaster *master, vector<FMIClient*>& clients, bool pushEverything) {
    //collect data
    control_proto::results_message results;
    map<FMIClient*, SendGetXType> clientVariables;

    results.set_version(1);
    results.set_step(step);
    results.set_t(t);
    results.set_t_end(endTime);
    results.set_dt(timeStep);

    for (auto client : clients) {
        const variable_map& vars = client->getVariables();
        SendGetXType getVariables;

        //figure out what values we need to fetch from the FMU
        for (auto var : vars) {
            //only put in values which are
            if (    pushEverything ||
                    var.second.causality == fmi2_causality_enu_input ||
                    var.second.causality == fmi2_causality_enu_output) {
                getVariables[var.second.type].push_back(var.second.vr);
            }
        }

        client->sendGetX(getVariables);
        clientVariables[client] = getVariables;
    }

    master->wait();

    for (auto cv : clientVariables) {
        control_proto::fmu_results *fmu_res = results.add_results();

        fmu_res->set_fmu_id(cv.first->getId());

        addVectorToRepeatedField(fmu_res->mutable_reals()->mutable_vrs(),       cv.second[fmi2_base_type_real]);
        addVectorToRepeatedField(fmu_res->mutable_reals()->mutable_values(),    cv.first->m_getRealValues);
        addVectorToRepeatedField(fmu_res->mutable_ints()->mutable_vrs(),        cv.second[fmi2_base_type_int]);
        addVectorToRepeatedField(fmu_res->mutable_ints()->mutable_values(),     cv.first->m_getIntegerValues);
        addVectorToRepeatedField(fmu_res->mutable_bools()->mutable_vrs(),       cv.second[fmi2_base_type_bool]);
        addVectorToRepeatedField(fmu_res->mutable_bools()->mutable_values(),    cv.first->m_getBooleanValues);
        addVectorToRepeatedField(fmu_res->mutable_strings()->mutable_vrs(),     cv.second[fmi2_base_type_str]);
        addVectorToRepeatedField(fmu_res->mutable_strings()->mutable_values(),  cv.first->m_getStringValues);
    }

    string str = results.SerializeAsString();
    zmq::message_t rep(str.length());
    memcpy(rep.data(), str.data(), str.length());
    push_socket.send(rep);
}

static int connectionNamesToVr(std::vector<connection> &connections,
                        vector<strongconnection> &strongConnections,
                        const vector<FMIClient*> clients // could not get this to compile when defined in parseargs
                        ){
    for(size_t i = 0; i < connections.size(); i++){
      connections[i].fromOutputVR = vrFromKeyName(clients[connections[i].fromFMU], connections[i].fromOutputVRorNAME);
      connections[i].toInputVR = vrFromKeyName(clients[connections[i].toFMU], connections[i].toInputVRorNAME);
    }

    for(size_t i = 0; i < strongConnections.size(); i++){
      size_t j = 0;
      if(strongConnections[i].vrORname.size()%2 != 0){
        fprintf(stderr,"Error: strong connection needs even number of connections for fmu0 and fmu1\n");
        exit(1);
      }
    }
  return 0;
}

#ifdef USE_MPI
void run_server(string fmuPath, jm_log_level_enu_t loglevel) {
    string hdf5Filename; //TODO?
    FMIServer server(fmuPath, loglevel >= jm_log_level_debug, loglevel, hdf5Filename);

    for (;;) {
        int rank, tag;
        std::string recv_str = mpi_recv_string(MPI_ANY_SOURCE, &rank, &tag);

        //shutdown command?
        if (tag == 1) {
            break;
        }

        //let Server handle packet, send reply back to master
        std::string str = server.clientData(recv_str.c_str(), recv_str.length());
        if (str.length() > 0) {
          MPI_Send((void*)str.c_str(), str.length(), MPI_CHAR, rank, tag, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
}
#endif

int main(int argc, char *argv[] ) {
#ifdef USE_MPI
    MPI_Init(NULL, NULL);

    //world = master at 0, FMUs at 1..N
    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#endif

    double timeStep = 0.1;
    double endTime = 10;
    double relativeTolerance = 0.0001;
    double relaxation = 4,
           compliance = 0;
    vector<string> fmuURIs;
    vector<connection> connections;
    parameter_map params;
    jm_log_level_enu_t loglevel = jm_log_level_nothing;
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
    Solver solver;
    string hdf5Filename;
    string fieldnameFilename;
    bool holonomic = true;
    int command_port = 0, results_port = 0;
    bool paused = false, running = true, solveLoops = false;

    if (parseArguments(
            argc, argv, &fmuURIs, &connections, &params, &endTime, &timeStep,
            &loglevel, &csv_separator, &outFilePath, &quietMode, &fileFormat,
            &method, &realtimeMode, &printXML, &stepOrder, &fmuVisibilities,
            &scs, &hdf5Filename, &fieldnameFilename, &holonomic, &compliance,
            &command_port, &results_port, &paused, &solveLoops)) {
        return 1;
    }

    bool zmqControl = command_port > 0 && results_port > 0;

    if (printXML) {
        fprintf(stderr, "XML mode not implemented\n");
        return 1;
    }

    if (quietMode) {
        fprintf(stderr, "WARNING: -q not implemented\n");
    }

    if (outFilePath != DEFAULT_OUTFILE) {
        fprintf(stderr, "WARNING: -o not implemented (output always goes to stdout)\n");
    }

#ifdef USE_MPI
    if (world_rank > 0) {
        //we're a server
        //in MPI mode, treat fmuURIs as a list of paths
        //for each server node, fmuURIs[world_rank-1] is the corresponding FMU path
        run_server(fmuURIs[world_rank-1], loglevel);
        return 0;
    }
    //world_rank == 0 below
#endif

    zmq::context_t context(1);

    zmq::socket_t rep_socket(context, ZMQ_REP);
    zmq::socket_t push_socket(context, ZMQ_PUSH);

    if (zmqControl) {
        fprintf(stderr, "Init zmq control on ports %i and %i\n", command_port, results_port);
        char addr[128];
        snprintf(addr, sizeof(addr), "tcp://*:%i", command_port);
        rep_socket.bind(addr);
        snprintf(addr, sizeof(addr), "tcp://*:%i", results_port);
        push_socket.bind(addr);
    } else if (paused) {
        fprintf(stderr, "-Z requires -z\n");
        return 1;
    }

#ifdef USE_MPI
    vector<FMIClient*> clients = setupClients(world_size-1);
#else
    //without this the maximum number of clients tops out at 300 on Linux,
    //around 63 on Windows (according to Web searches)
#ifdef ZMQ_MAX_SOCKETS
    zmq_ctx_set((void *)context, ZMQ_MAX_SOCKETS, fmuURIs.size() + (zmqControl ? 2 : 0));
#endif
    vector<FMIClient*> clients = setupClients(fmuURIs, context);
#endif

    //connect, get modelDescription XML (was important for connconf)
    for (auto it = clients.begin(); it != clients.end(); it++) {
        (*it)->m_loglevel = loglevel;
        (*it)->connect();
    }

    connectionNamesToVr(connections,scs,clients);
    vector<WeakConnection> weakConnections = setupWeakConnections(connections, clients);
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
        master->send(clients[x], fmi2_import_instantiate2( x < fmuVisibilities.size() ? fmuVisibilities[x] : false));
    }

    master->send(clients, fmi2_import_setup_experiment(true, relativeTolerance, 0, endTime >= 0, endTime));
    master->send(clients, fmi2_import_enter_initialization_mode());

    //send user-defined parameters
    sendUserParams(master, clients, params);

#ifdef WIN32
    LARGE_INTEGER freq, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);
#else
    timeval t1;
    gettimeofday(&t1, NULL);
#endif

    if (solveLoops) {
      //solve initial algebraic loops
      master->solveLoops();
    }

    //prepare solver and all that
    master->prepare();

    master->send(clients, fmi2_import_exit_initialization_mode());
    master->wait();

    //double t = 0;
    int step = 0;
    int nsteps = (int)round(endTime / timeStep);

#ifndef WIN32
    //HDF5
    int expected_records = 1+1.01*endTime/timeStep, nrecords = 0;
    timelog.reserve(expected_records*MAX_TIME_COLS);

    gettimeofday(&tl1, NULL);
#endif

    if (zmqControl) {
        pushResults(step, 0, endTime, timeStep, push_socket, master, clients, true);
    }

    //run
    while ((endTime < 0 || step < nsteps) && running) {
        double t = step * endTime / nsteps;

        if (zmqControl) {
            handleZmqControl(rep_socket, &paused, &running);
        }

        if (!running) {
            //termination requested
            break;
        }

        if (paused) {
            continue;
        }

#ifndef WIN32
        //HDF5
        columnofs = 0;
#endif
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
        }

        if (!zmqControl) {
            printOutputs(t, master, clients);
        }

        master->runIteration(t, timeStep);

        step++;

        if (zmqControl) {
            pushResults(step, t+timeStep, endTime, timeStep, push_socket, master, clients, false);
        } else {
            printf("\n");
        }

#ifndef WIN32
        //HDF5
        nrecords++;
#endif
    }

    if (!zmqControl) {
      printOutputs(endTime, master, clients);

      //finish off with zeroes for any extra forces
      int n = master->getNumForceOutputs();
      for (int i = 0; i < n; i++) {
        printf(",0");
      }

      printf("\n");
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
