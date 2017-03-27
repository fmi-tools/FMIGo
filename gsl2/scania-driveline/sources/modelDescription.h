/*This file is genereted by modeldescription2header. DO NOT EDIT! */
#ifndef MODELDESCRIPTION_H
#define MODELDESCRIPTION_H
#include "FMI2/fmi2Functions.h" //for fmi2Real etc.
#include "strlcpy.h" //for strlcpy()

#define MODEL_IDENTIFIER drivetrain_G5IO_m_function_only
#define MODEL_GUID "{df4d45a1-9637-428f-9266-e6cb20143adb}"
#define FMI_COSIMULATION

#define HAVE_DIRECTIONAL_DERIVATIVE 0
#define CAN_GET_SET_FMU_STATE 0
#define NUMBER_OF_REALS 39
#define NUMBER_OF_INTEGERS 1
#define NUMBER_OF_BOOLEANS 0
#define NUMBER_OF_STRINGS 0
#define NUMBER_OF_STATES 0
#define NUMBER_OF_EVENT_INDICATORS 0


#define HAVE_MODELDESCRIPTION_STRUCT
typedef struct {
    fmi2Real    current_time; //VR=0
    fmi2Real    w_inShaftNeutral; //VR=1
    fmi2Real    w_wheel; //VR=2
    fmi2Real    w_inShaftOld; //VR=3
    fmi2Real    tq_retarder; //VR=4
    fmi2Real    tq_fricLoss; //VR=5
    fmi2Real    tq_env; //VR=6
    fmi2Real    gear_ratio; //VR=7
    fmi2Real    tq_clutchMax; //VR=8
    fmi2Real    tq_losses; //VR=9
    fmi2Real    r_tire; //VR=10
    fmi2Real    m_vehicle; //VR=11
    fmi2Real    final_gear_ratio; //VR=12
    fmi2Real    w_eng; //VR=13
    fmi2Real    tq_eng; //VR=14
    fmi2Real    J_eng; //VR=15
    fmi2Real    J_neutral; //VR=16
    fmi2Real    tq_brake; //VR=17
    fmi2Real    ts; //VR=18
    fmi2Real    r_slipFilt; //VR=19
    fmi2Real    w_inShaftDer; //VR=20
    fmi2Real    w_wheelDer; //VR=21
    fmi2Real    tq_clutch; //VR=22
    fmi2Real    v_vehicle; //VR=23
    fmi2Real    w_out; //VR=24
    fmi2Real    w_inShaft; //VR=25
    fmi2Real    tq_outTransmission; //VR=26
    fmi2Real    v_driveWheel; //VR=27
    fmi2Real    r_slip; //VR=28
    fmi2Real    k1; //VR=30
    fmi2Real    f_shaft_out; //VR=31
    fmi2Real    w_shaft_in; //VR=32
    fmi2Real    w_wheel_out; //VR=33
    fmi2Real    f_wheel_in; //VR=34
    fmi2Real    k2; //VR=35
    fmi2Real    w_shaft_out; //VR=36
    fmi2Real    f_shaft_in; //VR=37
    fmi2Real    f_wheel_out; //VR=38
    fmi2Real    w_wheel_in; //VR=39
    fmi2Integer simulation_ticks; //VR=0


} modelDescription_t;


#define HAVE_DEFAULTS
static const modelDescription_t defaults = {
    0.000000, //current_time
    0.000000, //w_inShaftNeutral
    0.000000, //w_wheel
    0.000000, //w_inShaftOld
    0.000000, //tq_retarder
    0.000000, //tq_fricLoss
    0.000000, //tq_env
    0.000000, //gear_ratio
    0.000000, //tq_clutchMax
    0.000000, //tq_losses
    0.000000, //r_tire
    0.000000, //m_vehicle
    0.000000, //final_gear_ratio
    0.000000, //w_eng
    0.000000, //tq_eng
    0.000000, //J_eng
    0.000000, //J_neutral
    0.000000, //tq_brake
    0.000000, //ts
    0.000000, //r_slipFilt
    0.000000, //w_inShaftDer
    0.000000, //w_wheelDer
    0.000000, //tq_clutch
    0.000000, //v_vehicle
    0.000000, //w_out
    0.000000, //w_inShaft
    0.000000, //tq_outTransmission
    0.000000, //v_driveWheel
    0.000000, //r_slip
    0.000000, //k1
    0.000000, //f_shaft_out
    0.000000, //w_shaft_in
    0.000000, //w_wheel_out
    0.000000, //f_wheel_in
    0.000000, //k2
    0.000000, //w_shaft_out
    0.000000, //f_shaft_in
    0.000000, //f_wheel_out
    0.000000, //w_wheel_in
    0, //simulation_ticks


};


#define VR_CURRENT_TIME 0
#define VR_W_INSHAFTNEUTRAL 1
#define VR_W_WHEEL 2
#define VR_W_INSHAFTOLD 3
#define VR_TQ_RETARDER 4
#define VR_TQ_FRICLOSS 5
#define VR_TQ_ENV 6
#define VR_GEAR_RATIO 7
#define VR_TQ_CLUTCHMAX 8
#define VR_TQ_LOSSES 9
#define VR_R_TIRE 10
#define VR_M_VEHICLE 11
#define VR_FINAL_GEAR_RATIO 12
#define VR_W_ENG 13
#define VR_TQ_ENG 14
#define VR_J_ENG 15
#define VR_J_NEUTRAL 16
#define VR_TQ_BRAKE 17
#define VR_TS 18
#define VR_R_SLIPFILT 19
#define VR_W_INSHAFTDER 20
#define VR_W_WHEELDER 21
#define VR_TQ_CLUTCH 22
#define VR_V_VEHICLE 23
#define VR_W_OUT 24
#define VR_W_INSHAFT 25
#define VR_TQ_OUTTRANSMISSION 26
#define VR_V_DRIVEWHEEL 27
#define VR_R_SLIP 28
#define VR_K1 30
#define VR_F_SHAFT_OUT 31
#define VR_W_SHAFT_IN 32
#define VR_W_WHEEL_OUT 33
#define VR_F_WHEEL_IN 34
#define VR_K2 35
#define VR_W_SHAFT_OUT 36
#define VR_F_SHAFT_IN 37
#define VR_F_WHEEL_OUT 38
#define VR_W_WHEEL_IN 39
#define VR_SIMULATION_TICKS 0



//the following getters and setters are static to avoid getting linking errors if this file is included in more than one place

#define HAVE_GENERATED_GETTERS_SETTERS  //for letting the template know that we have our own getters and setters



static fmi2Status generated_fmi2GetReal(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_CURRENT_TIME: value[i] = md->current_time; break;
        case VR_W_INSHAFTNEUTRAL: value[i] = md->w_inShaftNeutral; break;
        case VR_W_WHEEL: value[i] = md->w_wheel; break;
        case VR_W_INSHAFTOLD: value[i] = md->w_inShaftOld; break;
        case VR_TQ_RETARDER: value[i] = md->tq_retarder; break;
        case VR_TQ_FRICLOSS: value[i] = md->tq_fricLoss; break;
        case VR_TQ_ENV: value[i] = md->tq_env; break;
        case VR_GEAR_RATIO: value[i] = md->gear_ratio; break;
        case VR_TQ_CLUTCHMAX: value[i] = md->tq_clutchMax; break;
        case VR_TQ_LOSSES: value[i] = md->tq_losses; break;
        case VR_R_TIRE: value[i] = md->r_tire; break;
        case VR_M_VEHICLE: value[i] = md->m_vehicle; break;
        case VR_FINAL_GEAR_RATIO: value[i] = md->final_gear_ratio; break;
        case VR_W_ENG: value[i] = md->w_eng; break;
        case VR_TQ_ENG: value[i] = md->tq_eng; break;
        case VR_J_ENG: value[i] = md->J_eng; break;
        case VR_J_NEUTRAL: value[i] = md->J_neutral; break;
        case VR_TQ_BRAKE: value[i] = md->tq_brake; break;
        case VR_TS: value[i] = md->ts; break;
        case VR_R_SLIPFILT: value[i] = md->r_slipFilt; break;
        case VR_W_INSHAFTDER: value[i] = md->w_inShaftDer; break;
        case VR_W_WHEELDER: value[i] = md->w_wheelDer; break;
        case VR_TQ_CLUTCH: value[i] = md->tq_clutch; break;
        case VR_V_VEHICLE: value[i] = md->v_vehicle; break;
        case VR_W_OUT: value[i] = md->w_out; break;
        case VR_W_INSHAFT: value[i] = md->w_inShaft; break;
        case VR_TQ_OUTTRANSMISSION: value[i] = md->tq_outTransmission; break;
        case VR_V_DRIVEWHEEL: value[i] = md->v_driveWheel; break;
        case VR_R_SLIP: value[i] = md->r_slip; break;
        case VR_K1: value[i] = md->k1; break;
        case VR_F_SHAFT_OUT: value[i] = md->f_shaft_out; break;
        case VR_W_SHAFT_IN: value[i] = md->w_shaft_in; break;
        case VR_W_WHEEL_OUT: value[i] = md->w_wheel_out; break;
        case VR_F_WHEEL_IN: value[i] = md->f_wheel_in; break;
        case VR_K2: value[i] = md->k2; break;
        case VR_W_SHAFT_OUT: value[i] = md->w_shaft_out; break;
        case VR_F_SHAFT_IN: value[i] = md->f_shaft_in; break;
        case VR_F_WHEEL_OUT: value[i] = md->f_wheel_out; break;
        case VR_W_WHEEL_IN: value[i] = md->w_wheel_in; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetReal(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_CURRENT_TIME: md->current_time = value[i]; break;
        case VR_W_INSHAFTNEUTRAL: md->w_inShaftNeutral = value[i]; break;
        case VR_W_WHEEL: md->w_wheel = value[i]; break;
        case VR_W_INSHAFTOLD: md->w_inShaftOld = value[i]; break;
        case VR_TQ_RETARDER: md->tq_retarder = value[i]; break;
        case VR_TQ_FRICLOSS: md->tq_fricLoss = value[i]; break;
        case VR_TQ_ENV: md->tq_env = value[i]; break;
        case VR_GEAR_RATIO: md->gear_ratio = value[i]; break;
        case VR_TQ_CLUTCHMAX: md->tq_clutchMax = value[i]; break;
        case VR_TQ_LOSSES: md->tq_losses = value[i]; break;
        case VR_R_TIRE: md->r_tire = value[i]; break;
        case VR_M_VEHICLE: md->m_vehicle = value[i]; break;
        case VR_FINAL_GEAR_RATIO: md->final_gear_ratio = value[i]; break;
        case VR_W_ENG: md->w_eng = value[i]; break;
        case VR_TQ_ENG: md->tq_eng = value[i]; break;
        case VR_J_ENG: md->J_eng = value[i]; break;
        case VR_J_NEUTRAL: md->J_neutral = value[i]; break;
        case VR_TQ_BRAKE: md->tq_brake = value[i]; break;
        case VR_TS: md->ts = value[i]; break;
        case VR_R_SLIPFILT: md->r_slipFilt = value[i]; break;
        case VR_W_INSHAFTDER: md->w_inShaftDer = value[i]; break;
        case VR_W_WHEELDER: md->w_wheelDer = value[i]; break;
        case VR_TQ_CLUTCH: md->tq_clutch = value[i]; break;
        case VR_V_VEHICLE: md->v_vehicle = value[i]; break;
        case VR_W_OUT: md->w_out = value[i]; break;
        case VR_W_INSHAFT: md->w_inShaft = value[i]; break;
        case VR_TQ_OUTTRANSMISSION: md->tq_outTransmission = value[i]; break;
        case VR_V_DRIVEWHEEL: md->v_driveWheel = value[i]; break;
        case VR_R_SLIP: md->r_slip = value[i]; break;
        case VR_K1: md->k1 = value[i]; break;
        case VR_F_SHAFT_OUT: md->f_shaft_out = value[i]; break;
        case VR_W_SHAFT_IN: md->w_shaft_in = value[i]; break;
        case VR_W_WHEEL_OUT: md->w_wheel_out = value[i]; break;
        case VR_F_WHEEL_IN: md->f_wheel_in = value[i]; break;
        case VR_K2: md->k2 = value[i]; break;
        case VR_W_SHAFT_OUT: md->w_shaft_out = value[i]; break;
        case VR_F_SHAFT_IN: md->f_shaft_in = value[i]; break;
        case VR_F_WHEEL_OUT: md->f_wheel_out = value[i]; break;
        case VR_W_WHEEL_IN: md->w_wheel_in = value[i]; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2GetInteger(const modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_SIMULATION_TICKS: value[i] = md->simulation_ticks; break;

        default: return fmi2Error;
        }
    }
    return fmi2OK;
}

static fmi2Status generated_fmi2SetInteger(modelDescription_t *md, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {
        int i;
    for (i = 0; i < nvr; i++) {
        switch (vr[i]) {
        case VR_SIMULATION_TICKS: md->simulation_ticks = value[i]; break;

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
