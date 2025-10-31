#ifndef CHEM_FEEDER_MQTT_H_
#define CHEM_FEEDER_MQTT_H_

#include <stdbool.h>
#include "aqualink.h"

#define CHEM_FEEDER_MQTT_TOPIC	"chem-feeder/topic"

void chem_feeder_mqtt_init(struct aqualinkdata *aqdata);
void chem_feeder_mqtt_process_msg(struct aqualinkdata *aqdata, void *data, int len);
bool is_chem_feeder_mqtt(char *topic, int len);

#endif
