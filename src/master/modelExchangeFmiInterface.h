
#define FMIGO_ME_SET_TIME(client) stepper->queueMessage(client, fmi2_import_set_time(t))
#define FMIGO_ME_SET_CONTINUOUS_STATES(client,data) stepper->queueMessage(client, fmi2_import_set_continuous_states(data + p->stepper->get_storage().get_offset(client->m_id, STORAGE::states), client->getNumContinuousStates()))
#define FMIGO_ME_GET_DERIVATIVES(client) stepper->queueMessage(client, fmi2_import_get_derivatives((int)client->getNumContinuousStates()))
#define FMIGO_ME_GET_EVENT_INDICATORS(client) stepper->queueMessage(client, fmi2_import_get_event_indicators((int)client->getNumEventIndicators()))
#define FMIGO_ME_GET_CONTINUOUS_STATES(client) stepper->queueMessage(client, fmi2_import_get_continuous_states((int)client->getNumContinuousStates()))


#define FMIGO_ME_ENTER_EVENT_MODE(clients) stepper->sendWait(clients, fmi2_import_enter_event_mode())
#define FMIGO_ME_NEW_DISCRETE_STATES(clients) stepper->sendWait(clients, fmi2_import_new_discrete_states())
#define FMIGO_ME_ENTER_CONTINUOUS_TIME_MODE(clients) stepper->sendWait(clients, fmi2_import_enter_continuous_time_mode())

#define FMIGO_ME_WAIT() stepper->wait()
