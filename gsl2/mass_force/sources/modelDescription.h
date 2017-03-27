/*This file is genereted by modeldescription2header. DO NOT EDIT! */
#ifndef MODELDESCRIPTION_H
#define MODELDESCRIPTION_H
#include "FMI2/fmi2Functions.h" //for fmi2Real etc.
#include "strlcpy.h" //for strlcpy()

#define MODEL_IDENTIFIER mass_force
#define MODEL_GUID "{829a504d-15f3-4312-a01f-ca78b6600f18}"
#define FMI_COSIMULATION

#define HAVE_DIRECTIONAL_DERIVATIVE 0
#define CAN_GET_SET_FMU_STATE 1
#define NUMBER_OF_REALS 6
#define NUMBER_OF_INTEGERS 1
#define NUMBER_OF_BOOLEANS 0
#define NUMBER_OF_STRINGS 0
#define NUMBER_OF_STATES 0
#define NUMBER_OF_EVENT_INDICATORS 0


#define HAVE_MODELDESCRIPTION_STRUCT
typedef struct {
    fmi2Real    force; //VR=0
    fmi2Real    force_c; //VR=1
    fmi2Real    x; //VR=2
    fmi2Real    v; //VR=3
    fmi2Real    damping; //VR=4
    fmi2Real    mass; //VR=5
    fmi2Integer filter_length; //VR=98


} modelDescription_t;


#define HAVE_DEFAULTS
static const modelDescription_t defaults = {
    0.000000, //force
    0.000000, //force_c
    0.000000, //x
    0.000000, //v
    0.000000, //damping
    1.000000, //mass
    0, //filter_length


};


#define VR_FORCE 0
#define VR_FORCE_C 1
#define VR_X 2
#define VR_V 3
#define VR_DAMPING 4
#define VR_MASS 5
#define VR_FILTER_LENGTH 98



//the following getters and setters are static to avoid getting linking errors if this file is included in more than one place

#define HAVE_GENERATED_GETTERS_SETTERS  //for letting the template know that we have our own getters and setters



static fmi2Status generated_fmi2GetReal(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_FORCE: value[i] = md->force; break;
        case VR_FORCE_C: value[i] = md->force_c; break;
        case VR_X: value[i] = md->x; break;
        case VR_V: value[i] = md->v; break;
        case VR_DAMPING: value[i] = md->damping; break;
        case VR_MASS: value[i] = md->mass; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetReal(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_FORCE: md->force = value[i]; break;
        case VR_FORCE_C: md->force_c = value[i]; break;
        case VR_X: md->x = value[i]; break;
        case VR_V: md->v = value[i]; break;
        case VR_DAMPING: md->damping = value[i]; break;
        case VR_MASS: md->mass = value[i]; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2GetInteger(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_FILTER_LENGTH: value[i] = md->filter_length; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetInteger(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_FILTER_LENGTH: md->filter_length = value[i]; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2GetBoolean(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetBoolean(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2GetString(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetString(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}
#endif //MODELDESCRIPTION_H
