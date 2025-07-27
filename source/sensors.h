#ifndef SENSORS_H_
#define SENSORS_H_

#include <stdbool.h>

#include "aqualink.h"

typedef struct external_sensor{
  char *path;
  float factor;
  char *label;
  float value;
  char ID[7];
  char *regex;
  char *uom;
} external_sensor;

bool read_sensor(external_sensor *sensor);
void stop_sensors_thread();
void start_sensors_thread(struct aqualinkdata *aq_data);

#endif