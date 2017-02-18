#ifndef SUNDIAL_INTERFACE_H
#define SUNDIAL_INTERFACE_H

#include <cvode/cvode.h>             /* prototypes for CVODE fcts., consts. */
#include <nvector/nvector_serial.h>  /* serial N_Vector types, fcts., macros */
#include <cvode/cvode_dense.h>       /* prototype for CVDense */
#include <sundials/sundials_dense.h> /* definitions DlsMat DENSE_ELEM */
#include <sundials/sundials_types.h> /* definition of type realtype */

/*****************************
 * Structure definitions
 *****************************
 */

typedef struct {
    realtype eps_rel;
    N_Vector eps_aps;
    //enum cgsl_step_control_ids id; /* control id */
    realtype start;			 /* optional initial step */
} csundial_step_control_parameters;
/**
 * Encapsulation of the integrator;
*/
typedef struct csundial_integrator{
    void* (*create)(int,int);
    int (*init)(void*,CVRhsFn,realtype,N_Vector);
    int (*tolerance)(void*, realtype, N_Vector);
    int (*rootInit)(void*, int,CVRootFn);
    int (*setJac)(void*,CVDlsDenseJacFn);
    //CVRhsFn function;
    //CVDlsDenseJacFn jacobian;
    //CVRootFn rootfinding;
    //N_Vector x;
} csundial_integrator;


//see <gsl/gsl_odeiv2.h> for an explanation of these
typedef int (* ode_function_ptr ) (double t, const double y[], double dydt[], void * params);
typedef int (* ode_jacobian_ptr ) (double t, const double y[], double * dfdy, double dfdt[], void * params);
typedef int (* pre_post_step_ptr ) (double t, double dt, const double y[], void * params);

/** Pre/post step callbacks
 *      t: Start of the current time step (communicationPoint)
 *     dt: Length of the current time step (communicationStepSize)
 *      y: For pre, the values of the variables before stepping. For post, after.
 * params: Opaque pointer
 */

/**
 *
 *  This is intended to contain everything needed to integrate only one
 *  little bit at a time *and* to be able to backtrack when needed, and to
 *  have multiple instances of the model.
 *
 *  Here we use this for inheritance by aggregation so that each model is
 *  declared as
 *  typedef struct my_model{
 *     cgs_model   m;
 *         .
 *         .
 *     extra stuff
 *         .
 *         .
 *  }  my_model ;
*/

typedef struct csundial_model{

    CVRhsFn function;
    CVDlsDenseJacFn jacobian;
    CVRootFn rootfinding;
    void (*free)(csundial_model *);
    N_Vector x;
    N_Vector abstol;
    realtype reltol;
} csundial_model;

/**
 * Useful function for allocating the most common type of model
 */
csundial_model* csundial_model_default_alloc(void);


/** Default destructor. Frees model->x and the model itself */
void csundial_model_default_free(csundial_model *model);

/**
 * Finally we can put everything in a bag.  The spefic model only has to
 * fill in these fields.
 */
typedef struct csundial_simulation {

    csundial_model *model;
    csundial_integrator i;
    void* cvode_mem;
    FILE * file;
    double t;		      /** current time */
    double t1;		      /** stop time */
    double h;		      /** first stepsize and current value */
    int    n;		      /** number of time steps taken */
    int    save;	      /** persistence to file */
} csundial_simulation;

/*****************************
 * Enum  definitions
 *****************************
 */

/**
 * List of available time integration methods in the gsl_odeiv2 library
 */
enum csundial_integrator_ids
{
  cvode,		/* 0 */
};

/**************
 * Function declarations.
 **************
 */

/**
 * Essentially the constructor for the csundial_simulation object.
 * The dynamical model is defined by a function, its Jacobian, an
 * opaque parameter struct, and a set of initial values.
 * All variables are assumed to be continuous.
 *
 *  \TODO: fix semantics for saving.  Use a filename
 *   instead of descriptor?  Open and close file automatically?
 */
csundial_simulation csundial_init_simulation(
    csundial_model * model, /** the model we work on */
    enum csundial_integrator_ids integrator, /** Integrator ID   */
    int save,
    FILE *g,
    csundial_step_control_parameters step_control);

/**
 *  Memory deallocation.
 */
void  csundial_free_simulation( csundial_simulation sim );

/**  Step from current time to next communication point. */
int csundial_step_to(void * _s,  double comm_point, double comm_step ) ;

/** Accessor */
const csundial_integrator csundial_get_integrator( int  i ) ;

/**  Commit to file. */
void csundial_save_data( struct csundial_simulation * sim );


/** Mutators for fixed step*/
/** \TODO: make this a toggle */
void csundial_simulation_set_fixed_step( csundial_simulation * s, double h );
void csundial_simulation_set_variable_step( csundial_simulation * s );

/** Get/set FMU state */
void csundial_simulation_get( csundial_simulation *s );
void csundial_simulation_set( csundial_simulation *s );

typedef int (*epce_post_step_ptr) (
    int n,                          /** Number of variables */
    const double outputs[],         /** Outputs. Filtered or not, depending on filter_length */
    void * params                   /** User pointer */
);

csundial_model * csundial_epce_model_init( csundial_model  *m, csundial_model *f,
        int filter_length,
        epce_post_step_ptr epce_post_step,
        void *epce_post_step_params
);

/**
 * Allocate an EPCE filtered model for the given model with the default filter:
 *
 *  zdot = x
 *
 * The corresponding Jacobian is the identity matrix.
 */
csundial_model * csundial_epce_default_model_init(
        csundial_model  *m,
        int filter_length,
        epce_post_step_ptr epce_post_step,
        void *epce_post_step_params
);

#endif
