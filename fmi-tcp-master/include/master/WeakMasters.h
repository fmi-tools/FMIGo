/*
 * WeakMasters.h
 *
 *  Created on: Aug 12, 2014
 *      Author: thardin
 */

#ifndef WEAKMASTERS_H_
#define WEAKMASTERS_H_
#include "master/BaseMaster.h"
#include "master/WeakConnection.h"
#include "common/common.h"
#include <fmitcp/serialize.h>
#include <FMI2/fmi2_types.h>
#include "common/gsl_interface.h"
//#include <sys/stat.h>
//#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace fmitcp::serialize;

namespace fmitcp_master {

//aka parallel stepper
class JacobiMaster : public BaseMaster {
public:
    JacobiMaster(vector<FMIClient*> clients, vector<WeakConnection> weakConnections) :
            BaseMaster(clients, weakConnections) {
        fprintf(stderr, "JacobiMaster\n");
    }

    void prepare() {
    }

    void runIteration(double t, double dt) {
        //get connection outputs
        for (auto it = clientWeakRefs.begin(); it != clientWeakRefs.end(); it++) {
            it->first->sendGetX(it->second);
        }
        wait();

        //print values
        /*for (auto it = clientWeakRefs.begin(); it != clientWeakRefs.end(); it++) {
            int i = 0;
            for (auto it2 = it->first->m_getRealValues.begin(); it2 != it->first->m_getRealValues.end(); it2++, i++) {
                fprintf(stderr, "%i real VR %i = %f\n", it->first->getId(), it->second[i], *it2);
            }
        }*/

        //set connection inputs, pipeline with do_step()
        const InputRefsValuesType refValues = getInputWeakRefsAndValues(m_weakConnections);

        // redirect outputs to inputs
        for (auto it = refValues.begin(); it != refValues.end(); it++) {
            it->first->sendSetX(it->second);
        }

        // why not wait here?? wait()

        sendWait(m_clients, fmi2_import_do_step(0, 0, t, dt, true));

    }
};

//aka serial stepper
class GaussSeidelMaster : public BaseMaster {
    map<FMIClient*, OutputRefsType> clientGetXs;  //one OutputRefsType for each client
    std::vector<int> stepOrder;
public:
    GaussSeidelMaster(vector<FMIClient*> clients, vector<WeakConnection> weakConnections, std::vector<int> stepOrder) :
        BaseMaster(clients,weakConnections), stepOrder(stepOrder) {
        fprintf(stderr, "GSMaster\n");
    }

    void prepare() {
        for (size_t x = 0; x < m_weakConnections.size(); x++) {
            WeakConnection wc = m_weakConnections[x];
            clientGetXs[wc.to][wc.from][wc.conn.fromType].push_back(wc.conn.fromOutputVR);
        }
    }

    void runIteration(double t, double dt) {
        for (int o : stepOrder) {
            FMIClient *client = m_clients[o];

            for (auto it = clientGetXs[client].begin(); it != clientGetXs[client].end(); it++) {
                it->first->sendGetX(it->second);
            }
            wait();
            const SendSetXType refValues = getInputWeakRefsAndValues(m_weakConnections, client);
            client->sendSetX(refValues);
            sendWait(client, fmi2_import_do_step(0, 0, t, dt, true));
        }
    }
};

class ModelExchangeStepper : public BaseMaster {
    typedef struct TimeLoop
    {
        fmi2_real_t t_start, t_safe, dt_new, t_crossed, t_end;
    } TimeLoop;
    typedef struct backup
    {
        double t;
        unsigned long failed_steps;

        FILE* result_file;
        long size_of_file;

    } backup;
    typedef struct fmu_parameters{

        double t_ok;
        double t_past;
        int count;                    /* number of function evaluations */
        int nz;                       /* number of event indicators, from XML file */
        int nx;                       /* number of states, from XML file */
        int loggingOn;                /* loggingOn == true => logging is enabled */
        char separator;               /* not used */
        const char* resultFile;       /* path and file name of where to store result */
        fmi2_string_t* categories;    /* only used in setDebugLogging */
        int nCategories;              /* only used in setDebugLogging */

        BaseMaster* baseMaster;       /* BaseMaster object pointer */
        FMIClient* client;            /* FMIClient object pointer */

      //fmi2_real_t t_start, t_end;//t_safe, t_new, t_crossed;
        fmi2_event_info_t event;

        bool stateEvent;
        backup m_backup;
    bool debuggingvar = false;
    }fmu_parameters;
    struct fmu_model{
      cgsl_model model;
    };
    vector<cgsl_simulation*> m_sims;
    vector<WeakConnection> m_weakConnections;
    enum INTEGRATORTYPE m_integratorType;
    double m_tolerance;
    TimeLoop timeLoop;
    bool reachedTheEnd = false;

    map<FMIClient*, OutputRefsType> clientGetXs;  //one OutputRefsType for each client
    std::vector<int> stepOrder;
 public:
 ModelExchangeStepper(vector<FMIClient*> clients, vector<WeakConnection> weakConnections, double relativeTolerance, enum INTEGRATORTYPE integratorType  ) :
    BaseMaster(clients,weakConnections), m_tolerance(relativeTolerance), m_integratorType(integratorType)
    {
      fprintf(stderr, "ModelExchangeStepper\n");
    }

    /** fmu_function
     *  function needed by cgsl_simulation to get and set the current
     *  states and derivatives
     *
     *  @param t Input time
     *  @param x Input states vector
     *  @param dxdt Output derivatives
     *  @param params Contains model specific parameters
     */
    static int fmu_function(double t, const double x[], double dxdt[], void* params)
    {

        // make local variables
        fmu_parameters* p = (fmu_parameters*)params;

        if(p->stateEvent)return GSL_SUCCESS;

        ++p->count; /* count function evaluations */
        p->baseMaster->send(p->client, fmi2_import_set_time(0,0,t));
        p->baseMaster->send(p->client, fmi2_import_set_continuous_states(0,0,x,p->nx));
        p->baseMaster->wait();
        p->baseMaster->sendWait(p->client, fmi2_import_get_derivatives(0,0,p->nx));
        p->baseMaster->get_storage().get_derivatives(dxdt, p->client->getId());

        if(t>0.45 && t<0.46)
            fprintf(stderr,"x0 = %f, t =%f\n", x[0],t);
        if(p->nz){
            p->baseMaster->sendWait(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
            if(p->baseMaster->get_storage().past_event(p->client->getId())){
                p->stateEvent = true;
                p->t_past = t;
                //fprintf(stderr,"fmu_function \n %0.32f,\n %0.32f\n", p->t_ok,p->t_past);
                //cout << x[0] << endl;
                p->debuggingvar = true;
                return GSL_CONTINUE;
            } else{
                p->stateEvent = false;
                p->t_ok = t;
            }
        }
        return GSL_SUCCESS;
    }

    /** getParameters
     *  Extracts the parameters from the model
     *
     *  @param m Model
     */
    fmu_parameters* getParameters(cgsl_model* m){
        return (fmu_parameters*)(m->parameters);
    }
    fmu_parameters* getParameters(fmu_model* m){
        return (fmu_parameters*)(m->model.parameters);
    }

    /** freeFMUModel
     *  Free all memory allocated by allacateMemory(fmu_model* m)
     *
     *  @param m Pointer to a fmu_model
     */
    void freeFMUModel(fmu_model* m){
        if(m == NULL) return;

        fmu_parameters* p = getParameters(m);
        if( p != NULL)             return;
        //if( p->resultFile != NULL) free(p->resultFile);
        if( m->model.x != NULL)    free(m->model.x);

        free(p);
        free(m);
    }
    void freeSim(void){
      fmu_parameters *p;
      while(m_sims.size()){
        freeFMUModel((fmu_model *)m_sims[0]->model);
        free(m_sims[0]);
        m_sims.pop_back();
      }
    }

    /** allocate Memory
     *  Allocates memory needed by the fmu_model
     *
     *  @param m Pointer to a fmu_model
     */
    void allocateMemory(fmu_model* m){
        bool allocFail = false;
        fmu_parameters* p = getParameters(m);
        m->model.x    = (double*)calloc(p->nx, sizeof(double));

        if(!m->model.x){
            freeFMUModel(m);
            perror("WeakMaster:ModelExchange:allocateMemory ERROR -  could not allocate memory");
            exit(1);
        }
    }

    /** init_fmu_model
     *  Creates the fmu_model and sets the parameters
     *
     *  @param client A pointer to a fmi client
     */
    cgsl_model* init_fmu_model(FMIClient* client){
        fmu_parameters* p = (fmu_parameters*)malloc(sizeof(fmu_parameters));
        fmu_model* m = (fmu_model*)malloc(sizeof(fmu_model));

        m->model.parameters = (void*)p;
        p->client = client;
        // TODO one result file for each fmu

        ostringstream prefix;
        prefix << "data/resultFile" << client->getId() << client->getSpaceSeparatedFieldNames("") <<".mat";
        //p->resultFile = prefix.str().c_str();//getModelResultPath(prefix.str());
        if ( ( p->m_backup.result_file = fopen(prefix.str().c_str(), "w+") ) == NULL){
            fprintf(stderr,"can't open file: %s\n", p->resultFile);
            exit(1);
        }
        p->baseMaster = this;
        p->nx = p->client->getNumContinuousStates();
        p->nz = p->client->getNumEventIndicators();
        m->model.n_variables = p->nx;

        fprintf(stderr,"\n------------------------\ninit_fmu_model debugging\n------------------------\n");
        fprintf(stderr,"nx         = %d\n",p->nx);
        fprintf(stderr,"n_variables= %d\n",m->model.n_variables);
        fprintf(stderr,"nz         = %d\n",p->nz);
        //fprintf(stderr,"resultPath = %s\n",p->resultFile);
        fprintf(stderr,"resultPath = %s\n",p->client->getSpaceSeparatedFieldNames("fmu").c_str());


        allocateMemory(m);

        m->model.function = fmu_function;
        m->model.jacobian = NULL;
        m->model.free = NULL;//freeFMUModel;
        sendWait(p->client, fmi2_import_get_continuous_states(0,0,p->nx));
        get_storage().get_states(m->model.x, p->client->getId());

        return(cgsl_model*)m;
    }


    void prepare() {
#ifdef USE_GPL
        /* This is the step control which determines tolerances. */
        cgsl_step_control_parameters step_control;
        step_control.eps_rel = 1e-6;
        step_control.eps_abs = 1e-6;
        step_control.id = step_control_y_new;
        step_control.start = 1e-8;

        fmu_alloc();
        for(FMIClient* client: m_clients)
          {
            cgsl_model* cgsl = init_fmu_model(client);
            fmu_parameters* p = getParameters(cgsl);
            cgsl_simulation* sim = (cgsl_simulation *)malloc(sizeof(cgsl_simulation));
            *sim = cgsl_init_simulation(cgsl,  /* model */
                                                       rk8pd, /* integrator: Runge-Kutta Prince Dormand pair order 7-8 */
                                                       1,     /* write to file: YES! */
                                                       p->m_backup.result_file,
                                                       step_control);
            m_sims.push_back(sim);
          }
        get_storage().sync();
        clerePrintFile();
#endif
    }

    /** restoreStates
     *  restores all values needed by the simulations to restart
     *  from a known safe time.
     *
     *  @param sims Vector of all simulations
     */
    void restoreStates(std::vector<cgsl_simulation*> &sims){
        get_storage().cycle();
        get_storage().sync();
        for(auto sim : sims) {
            fmu_parameters* p = getParameters(sim->model);
            //restore previous states
            get_storage().get_states(sim->model->x,p->client->getId());

            sim->i.evolution->failed_steps = p->m_backup.failed_steps;
            sim->t = p->m_backup.t;

            // reset position in the result file
            fseek(p->m_backup.result_file, p->m_backup.size_of_file, SEEK_SET);

            // truncate the result file to have the previous result file size
            int trunc = ftruncate(fileno(p->m_backup.result_file), p->m_backup.size_of_file);
            if(0 > trunc )
                perror("storeStates: ftruncate");
        }
    }

    /** storeStates
     *  stores all values needed by the simulations to restart
     *  from a known safe time.
     *
     *  @param sims Vector of all simulations
     */
    void storeStates(std::vector<cgsl_simulation*> &sims){
        for(auto sim : sims){
            fmu_parameters* p = getParameters(sim->model);
            send(p->client, fmi2_import_get_continuous_states(0,0,p->nx));
            p->m_backup.failed_steps = sim->i.evolution->failed_steps;
            p->m_backup.t = sim->t;
            p->m_backup.size_of_file = ftell(p->m_backup.result_file);
        }
        wait();
        for(auto sim : sims){
            fmu_parameters* p = getParameters(sim->model);
            get_storage().get_states(sim->model->x,p->client->getId());
            send(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
        }
        wait();
        get_storage().sync();

    }


    /** resetIntegratorTimeVariables:
     *  restore values of t_
     *
     *  @param t_safe A known safe time
     *  @param t_new A new time to try to reach
     */
    void resetIntegratorTimeVariables(double t_new)
    {
        // all simulations should be at the same time
        timeLoop.t_safe = m_sims[0]->t;
        timeLoop.t_crossed = timeLoop.t_end;
        timeLoop.dt_new = t_new;//min(sim->h, max(max((sim->t - prevTimeEvent)/500,0),t_end));
    }


    /** getStateEvent: Tries to retrieve event indicators, if successful the signbit of all
     *  event indicators z are compaired with event indicators in p->z
     *
     *  @param sims Vector of all simulations
     */
    void getStateEvent(std::vector<cgsl_simulation*> &sims){
        bool ret = false;

        fmu_parameters *p;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            sendWait(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
        }
        //fprintf(stderr,"getStateEvent ");
        //        get_storage().print(get_storage().get_indicators());
        //get_storage().print(get_storage().get_backup_indicators());

        size_t i = 0;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            p->stateEvent = get_storage().past_event(p->client->getId());
        }
    }

    /** hasStateEvent:
     ** returns true if at least one simulation has an event
     *
     *  @param sims Vector of all simulations
     */
    bool hasStateEvent(std::vector<cgsl_simulation*> &sims){
      fmu_parameters* p;
      for(auto sim:sims){
        p = getParameters(sim->model);
        if(p->stateEvent)
          return true;
      }
      return false;

    }

    /** getSafeTime:
     *  caluclates a "safe" time, uses the golden ratio to get
     *  t_crossed and t_safe to converge towards same value
     *
     *  @param sim A cgsl simulation
     */
    double getGoldenNewTime(std::vector<cgsl_simulation*> &sims){

        // golden ratio
        double phi = (1 + sqrt(5)) / 2;

        /* passed solution, need to reduce tEnd */
        if(hasStateEvent(sims)){
            //cout << "has " << endl;
            timeLoop.t_crossed = sims[0]->t;
            timeLoop.dt_new = (timeLoop.t_crossed - timeLoop.t_safe) / phi;

        } else { // havent passed solution, increase step
            //cout << "nhas" << endl;
            timeLoop.t_safe = sims[0]->t;
            timeLoop.dt_new = (timeLoop.t_crossed - timeLoop.t_safe)*(1.0 - 1.0 / phi);
        }

        return timeLoop.dt_new;
    }

    /** step
     *  run cgsl_step_to on all simulations
     *
     *  @param sims Vector of all simulations
     */
    void step(std::vector<cgsl_simulation*> &sims){
        fmu_parameters *p;
        for(auto sim: sims){
            p = getParameters(sim->model);
            p->stateEvent = false;
            p->t_past = max(p->t_past, sim->t + timeLoop.dt_new);
            p->t_ok = sim->t;
            if(p->t_ok > 1.5377 && 0)
                    fprintf(stderr,"step %0.32f, %0.32f\n", p->t_ok,p->t_past);
            cgsl_step_to(sim, sim->t, sim->t + timeLoop.dt_new);
        }
    }

    /** reduceSims
     *  Forget about all simulations that will not reach the event
     *  during current time step.
     *
     *  @param sims Vector of all simulations
     */
    void reduceSims(std::vector<cgsl_simulation*> &sims){
      fmu_parameters* p;
        for(int it = 0; it < sims.size();it++){
          p = getParameters((sims)[it]->model);
          if(!p->stateEvent)
            sims.erase(sims.begin() + it--);
        }
    }
    void clerePrintFile(){
        std::ofstream ofs("data/eventLocations.mat", std::ofstream::out | std::ofstream::trunc);
        ofs.close();
    }
    void printEventTimeToFile(double t, double z){
        ofstream fileOUT("data/eventLocations.mat", ios::app); // open filename.txt in append mode
        fileOUT << t << " " << z << endl; // append "some stuff" to the end of the file
        fileOUT.close(); // close the file
    }
    /** findEventTime
     *  if there is an event, find the event and return
     *  the time at where the time event occured
     *
     *  @param sims Vector of all simulations
     *  @return Returns the time immediatly after the event
     */
    void stepToEvent(std::vector<cgsl_simulation*> &sims){
        //resetIntegratorTimeVariables(timeLoop.t_crossed);
        //restoreStates(sims);
        int iter = 20;
        double tol = 1e-9;
        //set t_safe and t_crossed to values recived by gsl
        getSafeAndCrossed();
        cout.precision(16);
        while(get_storage().absmin(get_storage().get_indicators()) > tol &&
               timeLoop.dt_new > tol) {
            getGoldenNewTime(sims);
            if(hasStateEvent(sims)){
                restoreStates(sims);
                //cout << " crossed indi " <<  get_storage().get_indicators()[0] << endl << " time " << sims[0]->t << endl;
            }
            else storeStates(sims);
            step(sims);
            if(--iter < 0){
                cout<< "infloop "<<fixed<< timeLoop.t_crossed << endl << timeLoop.t_safe << endl;
                cout<< "infloop " << get_storage().get_backup_indicators()[0]<< endl;
            }
        }
        if(!hasStateEvent(sims)){
            timeLoop.dt_new = timeLoop.t_crossed - timeLoop.t_safe;
            step(sims);
            if(!hasStateEvent(sims)){
                cout << "ERROR" << endl;
                exit(55);
            }

        }
        storeStates(sims);
        cout << " have result " << get_storage().get_indicators()[0] << endl;
        printEventTimeToFile(sims[0]->t, get_storage().get_indicators()[0]);
        if(sims[0]->t > 2.5) exit(56);
    }

    /** reachedEnd
     *  @param t The current time
     *  @param pt Previuous time
     *  @param pc Count of how many times the different t - pt < toleranc
     */
    void reachedEnd(double t, double* pt, int* pc){
      double tol = 1e-2;
      if( t - *pt < tol ) (*pc)++;
      else *pc = 0;
      *pt = t;
    }

    /** newDiscreteStatesStart
     *  Should be used where a new discrete state ends
     *  and another begins. Resets the loop variables
     *  and store all states of the simulation
     *
     *  @param t The current time
     *  @param t_new New next time
     */
    void newDiscreteStatesStart(){
        // start at a new state
        sendWait(m_clients, fmi2_import_enter_event_mode(0,0));
        // todo loop until newDiscreteStatesNeeded == false
        sendWait(m_clients, fmi2_import_new_discrete_states(0,0));
        sendWait(m_clients, fmi2_import_enter_continuous_time_mode(0,0));
        fmu_parameters *p;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            send(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
        }
        wait();

        // store the current state of all running FMUs
        storeStates(m_sims);
    }

    void printStates(void){
      fmu_parameters* p;
      for(auto sim: m_sims){
        p = getParameters(sim->model);
        //sendWait(p->client,fmi2_import_get_continuous_states(0,0,p->nx));
        //sendWait(p->client,fmi2_import_get_event_indicators(0,0,p->nz));
      }
      fprintf(stderr,"      states     ");
      get_storage().print(get_storage().get_states());
      fprintf(stderr,"      indicator  ");
      get_storage().print(get_storage().get_indicators());
      fprintf(stderr,"      bindicator ");
      get_storage().print(get_storage().get_backup_indicators());
    }
    void getSafeAndCrossed(){
        fmu_parameters *p;
        p = getParameters(m_sims[0]->model);
        double t_ok = p->t_ok;
        double t_past = p->t_past;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            t_ok   = min( p->t_ok,    t_ok);
            t_past = min( p->t_past, t_past);
        }
        timeLoop.t_safe    = max( timeLoop.t_safe,    t_ok);
        timeLoop.t_crossed = min( timeLoop.t_crossed, t_past);
    }

    double safeTimeStep(std::vector<cgsl_simulation*> &sims){
        double t = sims[0]->t;
        double h = sims[0]->h;
        for(auto sim: sims) h = min(h,sim->h);

        double absmin = get_storage().absmin(get_storage().get_indicators());
        if(hasStateEvent(sims)){
            timeLoop.dt_new = h * (absmin > 0 ? absmin:0.00001);
            fprintf(stderr,"set a variable stepsize h %f, dt = %f\n",h,timeLoop.dt_new);
        }else
            timeLoop.dt_new = timeLoop.t_end - t;
        /* timeLoop.dt_new = min(timeLoop.t_end - t, */
        /*                       min(timeLoop.t_crossed - timeLoop.t_safe, */
        /*                           h)); */
    }

    void update_(){
        fmu_parameters *p;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            send(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
            send(p->client, fmi2_import_get_continuous_states(0,0,p->nx));
        }
        wait();
    }
    int breakit = 1000;

    void runIteration(double t, double dt) {
        if(reachedTheEnd) return;
        timeLoop.t_start = t;
        timeLoop.t_end = t + dt;
        double prevTimeEvent = t;
        int prevTimeCount = 0;

        fmu_parameters *p;
        for(auto sim: m_sims){
            p = getParameters(sim->model);
            p->stateEvent = false;
            p->t_ok = t;
            p->t_past = t + dt;
            sendWait(p->client, fmi2_import_get_continuous_states(0,0,p->nx));
            //sendWait(p->client, fmi2_import_get_derivatives(0,0,p->nx));
            sendWait(p->client, fmi2_import_get_event_indicators(0,0,p->nz));
        }

        //newDiscreteStatesStart(timeLoop.t_end);

        newDiscreteStatesStart();
        while( timeLoop.t_safe < timeLoop.t_end ){
            if (hasStateEvent(m_sims)){
                getSafeAndCrossed();// set crossed and safe time to time found by fmu_function

                restoreStates(m_sims);
                timeLoop.dt_new = timeLoop.t_safe - m_sims[0]->t;
                //if(m_sims[0]->t > 1.5377)
                //get_storage().print(get_storage().get_indicators()),
                //get_storage().print(get_storage().get_backup_indicators());
                //fprintf(stderr,"%f \n",*min_element(get_storage().get_indicators().begin(),get_storage().get_indicators().end()));
                //if(abs(*min_element(get_storage().get_indicators().begin(),get_storage().get_indicators().end())) > 1e-4)
                fmu_parameters*p=getParameters(m_sims[0]->model);
                if(get_storage().absmin(get_storage().get_indicators()) > 1e-6 && timeLoop.dt_new > 1e-6)
                    step(m_sims); // a safe step
                else
                    fprintf(stderr," has indicator size = %0.16f\n",get_storage().get_indicators()[0]);
                getSafeAndCrossed();
                if(hasStateEvent(m_sims)) restoreStates(m_sims);
                else storeStates(m_sims);

                stepToEvent(m_sims);
                //fprintf(stderr,"time after crossed step %f\n", m_sims[0]->t);
                ((fmu_parameters*)m_sims[0]->model->parameters)->debuggingvar = false;
            }else{
                timeLoop.t_safe = m_sims[0]->t;
                timeLoop.t_crossed = timeLoop.t_end;
                ((fmu_parameters*)m_sims[0]->model->parameters)->debuggingvar = false;
            }

            safeTimeStep(m_sims);
            if(hasStateEvent(m_sims))
               newDiscreteStatesStart();
            storeStates(m_sims);
            double t = m_sims[0]->t;
            step(m_sims);
            if(abs(timeLoop.dt_new-m_sims[0]->t + t) > 0)
                fprintf(stderr," t = %f, mt = %f,\ndt        = %0.32f \nactial dt = %0.32f\ndifference= %0.32f\n",t,m_sims[0]->t,timeLoop.dt_new,m_sims[0]->t-t,timeLoop.dt_new-m_sims[0]->t+t );

            if(((fmu_parameters*)m_sims[0]->model->parameters)->debuggingvar){
                fprintf(stderr,"used dt = %f \n",timeLoop.dt_new);
            }
        }
        //fprintf(stderr,"prevTimeCount = %d\n",prevTimeCount);
        //get_storage().print(get_storage().get_states());
    }
};
}// namespace fmitcp_master


#endif /* WEAKMASTERS_H_ */
