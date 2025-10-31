
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <pthread.h>
#include <string.h>
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>

#include "aqualink.h"
#include "chem_feeder_mqtt.h"

void chem_feeder_mqtt_init(struct aqualinkdata *aqdata)
{
	if (aqdata->ph == TEMP_UNKNOWN)
		aqdata->ph = 0;
}

void chem_feeder_mqtt_process_msg(struct aqualinkdata *aqdata, void *data, int len)
{
	char *msg = data;
	float ph = atof(msg);
	// bool active = strstr(msg, "on") != NULL ? true : false;
	// bool alarm = strstr(msg, "noalarm") != NULL ? false : true;
	if (aqdata->ph != ph) {
		aqdata->ph = ph;
		LOG(NET_LOG,LOG_DEBUG, "MQTT: Chem Feeder Device pH %0.1f\n", ph);
		SET_DIRTY(aqdata->is_dirty);
	}
}

bool is_chem_feeder_mqtt(char *topic, int len)
{
	return strncmp(topic, CHEM_FEEDER_MQTT_TOPIC, strlen(CHEM_FEEDER_MQTT_TOPIC)) == 0 ? true : false;
}
