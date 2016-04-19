#define MODEL_IDENTIFIER body
#define MODEL_GUID "{1fbe9e15-2bf7-4795-80da-a08eb447744f}"

enum {
    THETA,      //angle (output, state)
    OMEGA,      //angular velocity (output, state)
    ALPHA,      //angular acceleration (output)
    TAU,        //coupling torque (input)
    JINV,       //inverse of moment of inertia [1/(kg*m^2)] (parameter)
    D,          //drag (parameter)

    NUMBER_OF_REALS
};

#define NUMBER_OF_INTEGERS 0
#define NUMBER_OF_BOOLEANS 0
#define NUMBER_OF_STATES 0
#define NUMBER_OF_EVENT_INDICATORS 0
#define FMI_COSIMULATION

#include "fmuTemplate.h"

//returns partial derivative of vr with respect to wrt
static fmi2Status getPartial(state_t *s, fmi2ValueReference vr, fmi2ValueReference wrt, fmi2Real *partial) {
    if (vr == ALPHA && wrt == TAU) {
        *partial = s->r[JINV];
        return fmi2OK;
    }
    return fmi2Error;
}

static void doStep(state_t *s, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize) {
    s->r[ALPHA] =   s->r[JINV] * (s->r[TAU] - s->r[D]*s->r[OMEGA]);
    s->r[OMEGA] += s->r[ALPHA] * communicationStepSize;
    s->r[THETA] += s->r[OMEGA] * communicationStepSize;
}

// include code that implements the FMI based on the above definitions
#include "fmuTemplate_impl.h"
