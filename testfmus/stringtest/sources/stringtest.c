#include "modelDescription.h"

#define SIMULATION_INIT stringtest_init

#include "fmuTemplate.h"

static void stringtest_init(state_t *s) {
    strlcpy(s->md.s_in, s->md.s0, sizeof(s->md.s_in));
    strlcpy(s->md.s_out, s->md.s0, sizeof(s->md.s_out));
    strlcpy(s->md.s_in2, s->md.s02, sizeof(s->md.s_in2));
    strlcpy(s->md.s_out2, s->md.s02, sizeof(s->md.s_out2));
}

static void doStep(state_t *s, fmi2Real currentCommunicationPoint, fmi2Real communicationStepSize) {
    strlcpy(s->md.s_out, s->md.s_in, sizeof(s->md.s_out));
    strlcpy(s->md.s_out2, s->md.s_in2, sizeof(s->md.s_out2));
}

// include code that implements the FMI based on the above definitions
#include "fmuTemplate_impl.h"
