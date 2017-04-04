/*This file is genereted by modeldescription2header. DO NOT EDIT! */
#ifndef MODELDESCRIPTION_H
#define MODELDESCRIPTION_H
#include "FMI2/fmi2Functions.h" //for fmi2Real etc.
#include "strlcpy.h" //for strlcpy()
#include "modelExchange.h"

#define MODEL_IDENTIFIER wrapper_bouncingBall
#define MODEL_GUID "fd79201a-8655-4ab6-aef5-469c5ffe1fad"
#define FMI_COSIMULATION
#define HAVE_DIRECTIONAL_DERIVATIVE 0
#define CAN_GET_SET_FMU_STATE 1
#define NUMBER_OF_REALS 4
#define NUMBER_OF_INTEGERS 0
#define NUMBER_OF_BOOLEANS 0
#define NUMBER_OF_STRINGS 1
#define NUMBER_OF_STATES 0
#define NUMBER_OF_EVENT_INDICATORS 0


#define HAVE_MODELDESCRIPTION_STRUCT
typedef struct {
    fmi2Real h; //VR=0
    fmi2Real v; //VR=2
    fmi2Real g; //VR=4
    fmi2Real e; //VR=5
    fmi2Char    fmu[256]; //VR=0

} modelDescription_t;


#define HAVE_DEFAULTS
static const modelDescription_t defaults = {
    1.0, //h
    0.0, //v
    9.81, //g
    0.7, //e
    "sources/bouncingBall.fmu", //fmu

};


#define VR_H 0
#define VR_V 2
#define VR_G 4
#define VR_E 5
#define VR_FMU 0


//the following getters and setters are static to avoid getting linking errors if this file is included in more than one place

#define HAVE_GENERATED_GETTERS_SETTERS  //for letting the template know that we have our own getters and setters


static fmi2Status generated_fmi2GetReal(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
    fmi2_import_get_real(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2SetReal(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
    //if( *getfmi2Instance() != NULL)
        fmi2_import_set_real(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2GetInteger(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
    fmi2_import_get_integer(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2SetInteger(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
    //if( *getfmi2Instance() != NULL)
        fmi2_import_set_integer(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2GetBoolean(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    fmi2_import_get_boolean(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2SetBoolean(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    //if( *getfmi2Instance() != NULL)
        fmi2_import_set_boolean(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2GetString(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    fmi2_import_get_string(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}

static fmi2Status generated_fmi2SetString(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    //if( *getfmi2Instance() != NULL)
        fmi2_import_set_string(*getfmi2Instance(),vr,nvr,value);
    return fmi2OK;
}
#endif //MODELDESCRIPTION_H
