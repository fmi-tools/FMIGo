#include "sundial-interface.h"
#include <string.h>
#include <stdlib.h>

#include <cvode/cvode.h>             /* prototypes for CVODE fcts., consts. */
#include <nvector/nvector_serial.h>  /* serial N_Vector types, fcts., macros */
#include <cvode/cvode_dense.h>       /* prototype for CVDense */
#include <sundials/sundials_dense.h> /* definitions DlsMat DENSE_ELEM */
#include <sundials/sundials_types.h> /* definition of type realtype */

#ifdef WIN32
//http://stackoverflow.com/questions/6809275/unresolved-external-symbol-hypot-when-using-static-library#10051898
double hypot(double x, double y) {return _hypot(x, y);}
#endif
static int check_flag(void *flagvalue, const char *funcname, int opt);

const void * csundial_get_integrator( int  i ) {

  const void * integrators [] =
    {
    };

  return integrators [ i ];

}


/**
 * Integrate the equations by one full step.
 * Used by csundial_step_to() only, can't be external because we need comm_step for
 * being able to apply the ECPE filters (among other things).
 */
static int csundial_step ( void * _s  ) {
  return 0;

}

/**
 * Integrate the equations up to a given end point.
 */
int csundial_step_to(void * _s,  double comm_point, double comm_step ) {

  csundial_simulation * s = ( csundial_simulation * ) _s;

  int i;

  return 0;

}

#define NEQ   3                /* number of equations  */
#define RTOL  RCONST(1.0e-4)   /* scalar relative tolerance            */
#define T0    RCONST(0.0)      /* initial time           */
/**
 * Note: we use pass-by-value semantics for all structs anywhere possible.
 */
csundial_simulation csundial_init_simulation(
  csundial_model * model, /** the model we work on */
  double h,           //must be non-zero, even with variable step
  int fixed_step,     //if non-zero, use a fixed step of h
  int save,
  int print,
  int *f)
{
  csundial_simulation sim;
  realtype reltol;
  N_Vector y, abstol;
  int flag;
  /* Set the scalar relative tolerance */
  reltol = RTOL;

  y = N_VNew_Serial(NEQ);
  if (check_flag((void *)y, "N_VNew_Serial", 0)) exit(1);
  abstol = N_VNew_Serial(NEQ);
  if (check_flag((void *)abstol, "N_VNew_Serial", 0)) exit(1);

  sim.cvode_mem = NULL;
  sim.cvode_mem = CVodeCreate(CV_BDF, CV_NEWTON);
  if (check_flag((void *)sim.cvode_mem, "CVodeCreate", 0)) exit(1);

  /* Call CVodeInit to initialize the integrator memory and specify the
   * user's right hand side function in y'=f(t,y), the inital time T0, and
   * the initial dependent variable vector y. */
  flag = CVodeInit(sim.cvode_mem, model->function, T0, y);
  if (check_flag(&flag, "CVodeInit", 1)) exit(1);

  /* Call CVodeSVtolerances to specify the scalar relative tolerance
   * and vector absolute tolerances */
  flag = CVodeSVtolerances(sim.cvode_mem, reltol, abstol);
  if (check_flag(&flag, "CVodeSVtolerances", 1)) exit(1);
  return sim;

}

void csundial_model_default_free(csundial_model *model) {
}

static void csundial_model_default_get_state(csundial_model *model) {
}

static void csundial_model_default_set_state(csundial_model *model) {
}

csundial_model* csundial_model_default_alloc(int n_variables, const double *x0, void *parameters,
        ode_function_ptr function, ode_jacobian_ptr jacobian,
        pre_post_step_ptr pre_step, pre_post_step_ptr post_step, size_t sz) {

    csundial_model*m;
    return m;

}

void  csundial_free_simulation( csundial_simulation sim ) {

  return;
}

/** Resize an array by copying data, deallocate previous memory.  Return
 * new size */
static int gsl_hungry_alloc( int  n, double ** x ){

  return 0;

}

void csundial_save_data( struct csundial_simulation * sim ){

  return;

}

void csundial_simulation_set_fixed_step( csundial_simulation * s, double h){

  return;
}

void csundial_simulation_set_variable_step( csundial_simulation * s ) {

  return;
}

void csundial_simulation_get( csundial_simulation *s ) {
}

void csundial_simulation_set( csundial_simulation *s ) {
}




typedef struct csundial_epce_model {
  csundial_model  e_model;          /** this is what the csundial_simulation and csundial_integrator see */
  csundial_model  *model;           /** actual model */
  csundial_model  *filter;          /** filtered variables */

  double *z_prev;               /** z values of previous step, copied in pre_step
                                 * TODO: replace with circular buffer for longer averaging
                                 */
  double *z_prev_backup;        /** for get/set FMU state */
  double dt;                    /** current timestep */

  int filter_length;            /** EPCE filter length.
                                 * 0: No filtering applied (y = g(x))
                                 * 1: Use latest z only
                                 * 2: Use average of z and z_prev
                                 */

  epce_post_step_ptr epce_post_step;
  void *epce_post_step_params;
  double *outputs;
  double *filtered_outputs;

} csundial_epce_model;


static int csundial_epce_model_eval (double t, const double y[], double dydt[], void * params){
  return 0;
}

static int csundial_epce_model_jacobian (double t, const double y[], double * dfdy, double dfdt[], void * params){

    return 0;

}

static int csundial_epce_model_pre_step  (double t, double dt, const double y[], void * params){
    return 0;
}

static int csundial_epce_model_post_step (double t, double dt, const double y[], void * params){
    return 0;

}

static void csundial_epce_model_free(csundial_model *m) {
}

static void csundial_epce_model_get_state(csundial_model *model) {
}

static void csundial_epce_model_set_state(csundial_model *model) {
}

csundial_model * csundial_epce_model_init( csundial_model  *m, csundial_model *f,
        int filter_length,
        epce_post_step_ptr epce_post_step,
        void *epce_post_step_params){

    csundial_model* model;
  return (csundial_model*)model;
}

static int csundial_automatic_filter_function (double t, const double y[], double dydt[], void * params) {

    return 0;
}

static int csundial_automatic_filter_jacobian (double t, const double y[], double * dfdy, double dfdt[], void * params) {

    return 0;
}

csundial_model * csundial_epce_default_model_init(
        csundial_model  *m,
        int filter_length,
        epce_post_step_ptr epce_post_step,
        void *epce_post_step_params) {
    csundial_model*f;
    return csundial_epce_model_init(m, f, filter_length, epce_post_step, epce_post_step_params);
}

/*
 * Check function return value...
 *   opt == 0 means SUNDIALS function allocates memory so check if
 *            returned NULL pointer
 *   opt == 1 means SUNDIALS function returns a flag so check if
 *            flag >= 0
 *   opt == 2 means function allocates memory so check if returned
 *            NULL pointer
 */

static int check_flag(void *flagvalue, const char *funcname, int opt)
{
  int *errflag;

  /* Check if SUNDIALS function returned NULL pointer - no memory allocated */
  if (opt == 0 && flagvalue == NULL) {
    fprintf(stderr, "\nSUNDIALS_ERROR: %s() failed - returned NULL pointer\n\n",
	    funcname);
    return(1); }

  /* Check if flag < 0 */
  else if (opt == 1) {
    errflag = (int *) flagvalue;
    if (*errflag < 0) {
      fprintf(stderr, "\nSUNDIALS_ERROR: %s() failed with flag = %d\n\n",
	      funcname, *errflag);
      return(1); }}

  /* Check if function returned NULL pointer - no memory allocated */
  else if (opt == 2 && flagvalue == NULL) {
    fprintf(stderr, "\nMEMORY_ERROR: %s() failed - returned NULL pointer\n\n",
	    funcname);
    return(1); }

  return(0);
}
