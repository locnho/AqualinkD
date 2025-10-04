/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <regex.h>


#include "mongoose.h"
#include "aqualink.h"
#include "aq_scheduler.h"
#include "config.h"
#include "aq_panel.h"
//#include "utils.h"
#include "aq_systemutils.h"
#include "net_interface.h"


/*
Example /etc/cron.d/aqualinkd

01 10 1 * * curl localhost:80/api/Filter_Pump/set -d value=2 -X PUT
*/



bool passJson_scObj(const char* line, int length, aqs_cron *values)
{
  int keystart=0;
  //int keyend=0;
  int valuestart=0;
  int captured=0;
  bool readingvalue=false;
  bool invalue=false;
  //char value;
  values->enabled = true;

  //LOG(SCHD_LOG,LOG_DEBUG, "Obj body:'%.*s'\n", length, line);

  for (int i=0; i < length; i++) {
    if (line[i] == '}') {
      return (captured >= 7)?true:false;
    } else if (line[i] == '"' && keystart==0 && invalue==false && readingvalue==false) {
      keystart=i+1;
    } else if (line[i] == '"' && keystart > 0 && invalue==false && readingvalue==false) {
      //keyend=i;
    } else if (line[i] == ':' && keystart > 0 ) {
      invalue=true;
    } else if (line[i] == '"' && invalue == true && readingvalue == false && keystart > 0 ) {
      readingvalue=true;
      valuestart=i+1;
    } else if (line[i] == '"' && readingvalue == true) {
      // i is end of key
      if ( strncmp(&line[keystart], "enabled", 7) == 0) {
        values->enabled = (line[valuestart]=='0'?false:true);
        captured++;
      } else if ( strncmp(&line[keystart], "min", 3) == 0) {
        strncpy(values->minute, &line[valuestart], (i-valuestart) );
        values->minute[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "hour", 4) == 0) {
        strncpy(values->hour, &line[valuestart], (i-valuestart) );
        values->hour[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "daym", 4) == 0) {
        strncpy(values->daym, &line[valuestart], (i-valuestart) );
        values->daym[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "month", 5) == 0) {
        strncpy(values->month, &line[valuestart], (i-valuestart) );
        values->month[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "dayw", 4) == 0) {
        strncpy(values->dayw, &line[valuestart], (i-valuestart) );
        values->dayw[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "url", 3) == 0) {
        strncpy(values->url, &line[valuestart], (i-valuestart) );
        values->url[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "value", 5) == 0) {
        strncpy(values->value, &line[valuestart], (i-valuestart) );
        values->value[i-valuestart] = '\0';
        captured++;
      }
      keystart=0;
      //keyend=0;
      valuestart=0;
      invalue=false;
      readingvalue=false;
    }
  }

  return (captured >= 7)?true:false;
}




int save_schedules_js(const char* inBuf, int inSize, char* outBuf, int outSize)
{
  FILE *fp;
  int i;
  bool inarray = false;
  aqs_cron cline;
  bool fileexists = false;
  bool fs = false;
  const net_iface *iface = get_first_valid_interface();

  if ( !_aqconfig_.enable_scheduler) {
    LOG(SCHD_LOG,LOG_WARNING, "Schedules are disabled\n");
    return sprintf(outBuf, "{\"message\":\"Error Schedules disabled\"}");
  }

  LOG(SCHD_LOG,LOG_NOTICE, "Saving Schedule:\n");
  
  /*
  bool fs = remount_root_ro(false);
  if (access(CRON_FILE, F_OK) == 0)
    fileexists = true;
  fp = fopen(CRON_FILE, "w");
  */
  
  fp = aq_open_file( CRON_FILE, &fs, &fileexists);

  if (fp == NULL) {
    LOG(SCHD_LOG,LOG_ERR, "Open file failed '%s'\n", CRON_FILE);
    //remount_root_ro(true);
    aq_close_file(fp, fs);
    return sprintf(outBuf, "{\"message\":\"Error Saving Schedules\"}");
  }

  fprintf(fp, "#***** AUTO GENERATED DO NOT EDIT *****\n");
  fprintf(fp, "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin\n");

  LOG(SCHD_LOG,LOG_DEBUG, "Schedules Message body:\n'%.*s'\n", inSize, inBuf);

  for (i=0; i < inSize; i++) {
      if ( inBuf[i] == '[' ) {
        inarray=true;
      } else if ( inBuf[i] == ']' ) {
        inarray=false;
      } else if ( inarray && inBuf[i] == '{') {
        passJson_scObj( &inBuf[i], (inSize-i), &cline);
        LOG(SCHD_LOG,LOG_DEBUG, "Write to cron Min:%s Hour:%s DayM:%s Month:%s DayW:%s URL:%s Value:%s\n",cline.minute,cline.hour,cline.daym,cline.month,cline.dayw,cline.url,cline.value);
        //LOG(SCHD_LOG,LOG_INFO, "%s%s %s %s %s %s curl -s -S --show-error -o /dev/null localhost:%s%s -d value=%s -X PUT\n",(cline.enabled?"":"#"),cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, _aqconfig_.socket_port, cline.url, cline.value);
        //fprintf(fp, "%s%s %s %s %s %s root curl -s -S --show-error -o /dev/null localhost:%s%s -d value=%s -X PUT\n",(cline.enabled?"":"#"),cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, _aqconfig_.socket_port, cline.url, cline.value);
        LOG(SCHD_LOG,LOG_INFO, "%s%s %s %s %s %s curl -s -S --show-error %s -o /dev/null %s%s -d value=%s -X PUT\n",(cline.enabled?"":"#"),cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, (iface->isLocalurlTLS?"--insecure":""),(iface->localurl==NULL?_aqconfig_.listen_address:iface->localurl), cline.url, cline.value);
        fprintf(fp, "%s%s %s %s %s %s root curl -s -S --show-error %s -o /dev/null %s%s -d value=%s -X PUT\n",(cline.enabled?"":"#"),cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, (iface->isLocalurlTLS?"--insecure":""),(iface->localurl==NULL?_aqconfig_.listen_address:iface->localurl), cline.url, cline.value);      
      }
  }
    
  fprintf(fp, "#***** AUTO GENERATED DO NOT EDIT *****\n");

  //fclose(fp);

  // if we created file, change the permissions
  if (!fileexists)
    if ( chmod(CRON_FILE, S_IRUSR | S_IWUSR ) < 0 )
      LOG(SCHD_LOG,LOG_ERR, "Could not change permissions on cron file %s, scheduling may not work\n",CRON_FILE);

  //remount_root_ro(fs);
  aq_close_file(fp, fs);

  return sprintf(outBuf, "{\"message\":\"Saved Schedules\"}");
}

int build_schedules_js(char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  FILE *fp;
  char *line = NULL;
  int length = 0;
  int rc;
  aqs_cron cline;
  size_t len = 0;
  ssize_t read_size;
  regex_t regexCompiled;

  if ( !_aqconfig_.enable_scheduler) {
    LOG(SCHD_LOG,LOG_WARNING, "Schedules are disabled\n");
    if (size > 0)
      length += sprintf(buffer, "{\"message\":\"Error Schedules disabled\"}");
    return length;
  }

  // Below works for curl but not /usr/bin/curl in command.  NSF come back and fix the regexp
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(/api/.*)\\s-d value=([^\\d]+)\\s(.*)";
  // \d doesn't seem to be supported, so using [0-9]+ instead
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(\\/api\\/.*\\/set).* value=([0-9]+).*";
  
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(\\/api\\/.*\\/set).* value=([0-9]+).*";
  const char *regexString="(#{0,1})([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(\\/api\\/.*\\/set).* value=([0-9]+).*";

  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(/api/.*/set).*value=([0-9]+).*";


  size_t maxGroups = 15;
  regmatch_t groupArray[maxGroups];
  //static char buf[100];

  if (size > 0)
    length += sprintf(buffer+length,"{\"type\": \"schedules\",");

  if (0 != (rc = regcomp(&regexCompiled, regexString, REG_EXTENDED))) {
      LOG(SCHD_LOG,LOG_ERR, "regcomp() failed, returning nonzero (%d)\n", rc);
      if (size > 0)
        length += sprintf(buffer+length,"\"message\": \"Error reading schedules\"}");
      return length;
   }

  fp = fopen(CRON_FILE, "r");
  if (fp == NULL) {
    LOG(SCHD_LOG,LOG_WARNING, "Open file failed '%s'\n", CRON_FILE);
    if (size > 0)
      length += sprintf(buffer+length,"\"message\": \"Error reading schedules\"}");
    return length;
  }

  if (size > 0)
    length += sprintf(buffer+length,"\"schedules\": [ ");

  while ((read_size = getline(&line, &len, fp)) != -1) {
    //printf("Read from cron:-\n  %s", line);
    //lc++;
    //rc = regexec(&regexCompiled, line, maxGroups, groupArray, 0);
    if (0 == (rc = regexec(&regexCompiled, line, maxGroups, groupArray, REG_EXTENDED))) {
      // Group 1 is # (enable or not)
      // Group 2 is minute
      // Group 3 is hour
      // Group 4 is day of month
      // Group 5 is month
      // Group 6 is day of week
      // Group 7 is root
      // Group 8 is curl
      // Group 9 is URL
      // Group 10 is value
      if (groupArray[8].rm_so == (size_t)-1) {
        if (size > 0) {
          LOG(SCHD_LOG,LOG_ERR, "No matching information from cron file\n");
        }
      } else {
        cline.enabled = (line[groupArray[1].rm_so] == '#')?false:true;
        sprintf(cline.minute, "%.*s", (groupArray[2].rm_eo - groupArray[2].rm_so), (line + groupArray[2].rm_so));
        sprintf(cline.hour, "%.*s",   (groupArray[3].rm_eo - groupArray[3].rm_so), (line + groupArray[3].rm_so));
        sprintf(cline.daym, "%.*s",   (groupArray[4].rm_eo - groupArray[4].rm_so), (line + groupArray[4].rm_so));
        sprintf(cline.month, "%.*s",  (groupArray[5].rm_eo - groupArray[5].rm_so), (line + groupArray[5].rm_so));
        sprintf(cline.dayw, "%.*s",   (groupArray[6].rm_eo - groupArray[6].rm_so), (line + groupArray[6].rm_so)); 
        sprintf(cline.url, "%.*s",    (groupArray[9].rm_eo - groupArray[9].rm_so), (line + groupArray[9].rm_so));
        sprintf(cline.value, "%.*s",  (groupArray[10].rm_eo - groupArray[10].rm_so), (line + groupArray[10].rm_so));
        if (size > 0) {
          LOG(SCHD_LOG,LOG_INFO, "Read from cron. Enabled:%d Min:%s Hour:%s DayM:%s Month:%s DayW:%s URL:%s Value:%s\n",cline.enabled,cline.minute,cline.hour,cline.daym,cline.month,cline.dayw,cline.url,cline.value);
          length += sprintf(buffer+length, "{\"enabled\":\"%d\", \"min\":\"%s\",\"hour\":\"%s\",\"daym\":\"%s\",\"month\":\"%s\",\"dayw\":\"%s\",\"url\":\"%s\",\"value\":\"%s\"},",
                cline.enabled,
                cline.minute,
                cline.hour,
                cline.daym,
                cline.month,
                cline.dayw,
                cline.url,
                cline.value);
        }
        //LOG(SCHD_LOG,LOG_DEBUG, "Read from cron Day %d | Time %d:%d | Zone %d | Runtime %d\n",day,hour,minute,zone,runtime);

        // Test / get for pump start and end time
        if (isAQS_USE_CRON_PUMP_TIME_ENABLED) {
          // Could also check that dayw is *
          if ( cline.enabled && strstr(cline.url, AQS_PUMP_URL ))
          {
            int value = strtoul(cline.value, NULL, 10);
            int hour = strtoul(cline.hour, NULL, 10);
            if (value == 0) {
              if (hour > _aqconfig_.sched_chk_pumpoff_hour) // NSF this picks up the greatest offhour, (do we want the smallest???) 
                _aqconfig_.sched_chk_pumpoff_hour = hour;
            } else if (value == 1){
              if (hour < _aqconfig_.sched_chk_pumpon_hour || _aqconfig_.sched_chk_pumpon_hour == 0)
                _aqconfig_.sched_chk_pumpon_hour = hour;
            } 
          }
        }
      }
    } else {
      if (size > 0) {
        LOG(SCHD_LOG,LOG_DEBUG, "regexp no match (%d) %s", rc, line);
      }
    }
  }

  if (size > 0) {
    buffer[--length] = '\0';
    length += sprintf(buffer+length,"]}\n");
  }

  fclose(fp);
  regfree(&regexCompiled);

  return length;
}

void get_cron_pump_times()
{
  build_schedules_js(NULL, 0);
  return;
}


bool event_happened_set_device_state(reset_event_type type, struct aqualinkdata *aqdata)
{
  if (! isAQS_START_PUMP_EVENT_ENABLED) {
    LOG(SCHD_LOG,LOG_DEBUG, "Event scheduler is not enabled\n");
    return false;
  }
  // Check time is between hours.
  bool scheduledOn = false;

  if (isAQS_USE_CRON_PUMP_TIME_ENABLED) {
    get_cron_pump_times();
    LOG(SCHD_LOG,LOG_DEBUG, "Pump on times from scheduler are between hours %.2d & %.2d\n",_aqconfig_.sched_chk_pumpon_hour, _aqconfig_.sched_chk_pumpoff_hour);
  }
/*
  if (_aqconfig_.sched_chk_pumpon_hour == AQ_UNKNOWN || _aqconfig_.sched_chk_pumpoff_hour == AQ_UNKNOWN ) {
    if ( CRON TURNED OFF ) {
      LOG(SCHD_LOG,LOG_ERR, "No pump on / off times configures and cron scheduler not enabled, can't action event!");
      return false;
    }
    get_cron_pump_times();
    LOG(SCHD_LOG,LOG_DEBUG, "Pump on times from scheduler are between hours %.2d & %.2d\n",_aqconfig_.sched_chk_pumpon_hour, _aqconfig_.sched_chk_pumpoff_hour);

  }
*/
  time_t now = time(NULL);
  struct tm *tm_struct = localtime(&now);

  int hour = tm_struct->tm_hour;
  if (hour >= _aqconfig_.sched_chk_pumpon_hour && hour < _aqconfig_.sched_chk_pumpoff_hour ) {
    scheduledOn = true;
  }

  // Check event type.
  switch(type){
    case AQS_POWER_ON:
      if (scheduledOn && isAQS_POWER_ON_ENABED && aqdata->aqbuttons[0].led->state == OFF) {
        LOG(SCHD_LOG,LOG_INFO, "Powered on, schedule is set for pump running and pump is off, turning pump on\n");
        panel_device_request(aqdata, ON_OFF, 0, true, NET_TIMER);
      } else {
        //LOG(SCHD_LOG,LOG_DEBUG, "Powered on, schedule is not set and/or pump is already on, leaving\n");
        LOG(SCHD_LOG,LOG_DEBUG, "Powered on, schedule Pump on is %sset, time is %sbetween scheduled hours, Pump is %s, (not changing)\n",(isAQS_POWER_ON_ENABED?"":"not "),(scheduledOn?"":" not"), (aqdata->aqbuttons[0].led->state ==OFF?"Off":"On"));
      }
    break;
    case AQS_FRZ_PROTECT_OFF:
      if (scheduledOn && isAQS_FRZ_PROTECT_OFF_ENABED && aqdata->aqbuttons[0].led->state == OFF) {
        LOG(SCHD_LOG,LOG_INFO, "Freeze Protect off, schedule is set for pump running and pump is off, turning pump on\n");
        panel_device_request(aqdata, ON_OFF, 0, true, NET_TIMER);
      } else {
        //LOG(SCHD_LOG,LOG_DEBUG, "Freeze Protect off, schedule is not set and/or pump is already on, leaving\n");
        LOG(SCHD_LOG,LOG_DEBUG, "Freeze Protect off, schedule Pump on is %sset, time is %sbetween scheduled hours, Pump is %s, (not changing)\n",(isAQS_FRZ_PROTECT_OFF_ENABED?"":"not "),(scheduledOn?"":" not"), (aqdata->aqbuttons[0].led->state ==OFF?"Off":"On"));
      }
    break;
    case AQS_BOOST_OFF:
      if (scheduledOn && isAQS_BOOST_OFF_ENABED && aqdata->aqbuttons[0].led->state == OFF) {
        LOG(SCHD_LOG,LOG_INFO, "Boost off, schedule is set for pump running and pump is off, turning pump on\n");
        panel_device_request(aqdata, ON_OFF, 0, true, NET_TIMER);
      } else {
        //LOG(SCHD_LOG,LOG_DEBUG, "Boost off, schedule is not set and/or pump is already on, leaving\n");
        LOG(SCHD_LOG,LOG_DEBUG, "Boost off, schedule Pump on is %sset, time is %sbetween scheduled hours, Pump is %s, (not changing)\n",(isAQS_BOOST_OFF_ENABED?"":"not "),(scheduledOn?"":" not"), (aqdata->aqbuttons[0].led->state ==OFF?"Off":"On"));
      }

      if (aqdata->boost_linked_device != AQ_UNKNOWN && aqdata->boost_linked_device <= aqdata->total_buttons && aqdata->boost_linked_device >= 0) {
        //aqdata->aqbuttons[aqdata->boost_linked_device].code
        if (aqdata->aqbuttons[aqdata->boost_linked_device].led->state == OFF) {
          panel_device_request(aqdata, ON_OFF, aqdata->boost_linked_device, false, NET_TIMER);
          LOG(SCHD_LOG,LOG_INFO, "Boost off, Turing %s off\n",aqdata->aqbuttons[aqdata->boost_linked_device].label);
        } else {
          LOG(SCHD_LOG,LOG_INFO, "Boost off, %s is already off\n",aqdata->aqbuttons[aqdata->boost_linked_device].label);
        }
      }
    break;
    case AQS_BOOST_ON:
      if (aqdata->boost_linked_device != AQ_UNKNOWN && aqdata->boost_linked_device <= aqdata->total_buttons && aqdata->boost_linked_device >= 0) {
        //aqdata->aqbuttons[aqdata->boost_linked_device].code
        if (aqdata->aqbuttons[aqdata->boost_linked_device].led->state == OFF) {
          panel_device_request(aqdata, ON_OFF, aqdata->boost_linked_device, true, NET_TIMER);
          LOG(SCHD_LOG,LOG_INFO, "Boost on, Turing %s on\n",aqdata->aqbuttons[aqdata->boost_linked_device].label);
        } else {
          LOG(SCHD_LOG,LOG_INFO, "Boost on, %s is already on\n",aqdata->aqbuttons[aqdata->boost_linked_device].label);
        }
      }
    break;
  }

  return true;
}
