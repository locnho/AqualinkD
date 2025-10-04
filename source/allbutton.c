#define _GNU_SOURCE 1 // for strcasestr & strptime
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aqualink.h"
#include "allbutton.h"
#include "rs_msg_utils.h"
#include "devices_jandy.h"
#include "allbutton_aq_programmer.h"
#include "color_lights.h"
#include "aq_scheduler.h"

/* Below can also be called from serialadapter.c */
void processLEDstate(struct aqualinkdata *aqdata, unsigned char *packet, logmask_t from)
{

  int i = 0;
  int byte;
  int bit;

  if (memcmp(aqdata->raw_status, packet + 4, AQ_PSTLEN) != 0) {
    SET_DIRTY(aqdata->is_dirty);
    LOG(from,LOG_DEBUG, "Processing LEDs status CHANGED\n");
  } else {
    LOG(from,LOG_DEBUG, "Processing LEDs status\n");
    // Their is no point in continuing here, so we could return if wanted.
    // But for the moment, we don't need to speed up anything.
  }

  memcpy(aqdata->raw_status, packet + 4, AQ_PSTLEN);

  //debuglogPacket(ALLB_LOG, );
  

  for (byte = 0; byte < 5; byte++)
  {
    for (bit = 0; bit < 8; bit += 2)
    {
      if (((aqdata->raw_status[byte] >> (bit + 1)) & 1) == 1){
        //aqdata->aqualinkleds[i].state = FLASH;
        SET_IF_CHANGED(aqdata->aqualinkleds[i].state, FLASH, aqdata->is_dirty);
      } else if (((aqdata->raw_status[byte] >> bit) & 1) == 1){
        //aqdata->aqualinkleds[i].state = ON;
        SET_IF_CHANGED(aqdata->aqualinkleds[i].state, ON, aqdata->is_dirty);
      }else{
        //aqdata->aqualinkleds[i].state = OFF;
        SET_IF_CHANGED(aqdata->aqualinkleds[i].state, OFF, aqdata->is_dirty);
      }

      //LOG(from,LOG_DEBUG,"Led %d state %d",i+1,aqdata->aqualinkleds[i].state);
      i++;
    }
  }
  // Reset enabled state for heaters, as they take 2 led states
  if (aqdata->aqualinkleds[POOL_HTR_LED_INDEX - 1].state == OFF && aqdata->aqualinkleds[POOL_HTR_LED_INDEX].state == ON){
    //aqdata->aqualinkleds[POOL_HTR_LED_INDEX - 1].state = ENABLE;
    SET_IF_CHANGED(aqdata->aqualinkleds[POOL_HTR_LED_INDEX - 1].state, ENABLE, aqdata->is_dirty);
  }

  if (aqdata->aqualinkleds[SPA_HTR_LED_INDEX - 1].state == OFF && aqdata->aqualinkleds[SPA_HTR_LED_INDEX].state == ON){
    //aqdata->aqualinkleds[SPA_HTR_LED_INDEX - 1].state = ENABLE;
    SET_IF_CHANGED(aqdata->aqualinkleds[SPA_HTR_LED_INDEX - 1].state, ENABLE, aqdata->is_dirty);
  }

  if (aqdata->aqualinkleds[SOLAR_HTR_LED_INDEX - 1].state == OFF && aqdata->aqualinkleds[SOLAR_HTR_LED_INDEX].state == ON){
    //aqdata->aqualinkleds[SOLAR_HTR_LED_INDEX - 1].state = ENABLE;
    SET_IF_CHANGED(aqdata->aqualinkleds[SOLAR_HTR_LED_INDEX - 1].state, ENABLE, aqdata->is_dirty);
  }
  /*
  for (i=0; i < TOTAL_BUTTONS; i++) {
    LOG(from,LOG_NOTICE, "%s = %d", aqdata->aqbuttons[i].name,  aqdata->aqualinkleds[i].state);
  }
*/
#ifdef CLIGHT_PANEL_FIX // Use state from RSSD protocol for color light if it's on.
  for (int i=0; i < aqdata->num_lights; i++) {
    if ( aqdata->lights[i].RSSDstate == ON && aqdata->lights[i].button->led->state != ON ) {
      aqdata->lights[i].button->led->state = aqdata->lights[i].RSSDstate;
      SET_IF_CHANGED(aqdata->lights[i].button->led->state, aqdata->lights[i].RSSDstate, aqdata->is_dirty);
      //LOG(from,LOG_WARNING,"Fix Jandy bug, color light '%s' is on, setting status to match!\n", aqdata->lights[i].button->label);
    }
    
    // Below is for aqualinkd programmable light, set color mode to last if something else turns it on or off.
    if (aqdata->lights[i].lightType == LC_PROGRAMABLE && ! in_light_programming_mode(aqdata)) {
      if (aqdata->lights[i].button->led->state == OFF && aqdata->lights[i].currentValue != 0) {
        set_currentlight_value(&aqdata->lights[i], 0);
        //LOG(ALLB_LOG,LOG_NOTICE,"****** SET LIGHT MODE 0 ******\n");
      } else if (aqdata->lights[i].button->led->state == ON && aqdata->lights[i].currentValue == 0 && aqdata->lights[i].lastValue != 0) {
        set_currentlight_value(&aqdata->lights[i], aqdata->lights[i].lastValue);
        //LOG(ALLB_LOG,LOG_NOTICE,"****** SET LIGHT MODE %d ******\n",aqdata->lights[i].lastValue);
      }
    }
  }
#endif
}

void setUnits(char *msg, struct aqualinkdata *aqdata)
{
  char buf[AQ_MSGLEN*3];

  rsm_strncpy(buf, (unsigned char *)msg, AQ_MSGLEN*3, AQ_MSGLONGLEN);

  //ascii(buf, msg);
  LOG(ALLB_LOG,LOG_DEBUG, "Getting temp units from message '%s', looking at '%c'\n", buf, buf[strlen(buf) - 1]);

  if (msg[strlen(msg) - 1] == 'F') {
    //aqdata->temp_units = FAHRENHEIT;
    SET_IF_CHANGED(aqdata->temp_units, FAHRENHEIT, aqdata->is_dirty);
  } else if (msg[strlen(msg) - 1] == 'C') {
    //aqdata->temp_units = CELSIUS;
    SET_IF_CHANGED(aqdata->temp_units, CELSIUS, aqdata->is_dirty);
  } else {
    //aqdata->temp_units = UNKNOWN;
    SET_IF_CHANGED(aqdata->temp_units, UNKNOWN, aqdata->is_dirty);
  }

  LOG(ALLB_LOG,LOG_INFO, "Temp Units set to %d (F=0, C=1, Unknown=2)\n", aqdata->temp_units);
}

// Defined as int16_t so 16 bits to mask
#define MSG_FREEZE      (1 << 0) // 1
#define MSG_SERVICE     (1 << 1) // 1
#define MSG_SWG         (1 << 2)
#define MSG_BOOST       (1 << 3)
#define MSG_TIMEOUT     (1 << 4)
#define MSG_RS13BUTTON  (1 << 5)
#define MSG_RS14BUTTON  (1 << 6)
#define MSG_RS15BUTTON  (1 << 7)
#define MSG_RS16BUTTON  (1 << 8)
#define MSG_BATTERY_LOW (1 << 9)
#define MSG_SWG_DEVICE  (1 << 10)
#define MSG_LOOP_POOL_TEMP   (1 << 11)
#define MSG_LOOP_SPA_TEMP    (1 << 12)


int16_t  RS16_endswithLEDstate(char *msg, struct aqualinkdata *aqdata)
{
  char *sp;
  int i;
  aqledstate state = LED_S_UNKNOWN;

  //if (_aqconfig_.rs_panel_size < 16)
  if (PANEL_SIZE() < 16)
    return false;

  sp = strrchr(msg, ' ');

  if( sp == NULL )
    return false;
  
  if (strncasecmp(sp, " on", 3) == 0)
    state = ON;
  else if (strncasecmp(sp, " off", 4) == 0)
    state = OFF;
  else if (strncasecmp(sp, " enabled", 8) == 0) // Total guess, need to check
    state = ENABLE;
  else if (strncasecmp(sp, " no idea", 8) == 0) // need to figure out these states
    state = FLASH;

  if (state == LED_S_UNKNOWN)
    return false;

  // Only need to start at Aux B5->B8 (12-15)
  // Loop over only aqdata->aqbuttons[13] to aqdata->aqbuttons[16]
  for (i = aqdata->rs16_vbutton_start; i <= aqdata->rs16_vbutton_end; i++) {
    //TOTAL_BUTTONS
    if ( stristr(msg, aqdata->aqbuttons[i].label) != NULL) {
      //aqdata->aqbuttons[i].led->state = state;
      SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, state, aqdata->is_dirty);
      LOG(ALLB_LOG,LOG_INFO, "Set %s to %d\n", aqdata->aqbuttons[i].label, aqdata->aqbuttons[i].led->state);
      // Return true should be the result, but in the if we want to continue to display message
      //return true;
      if (i == 13)
        return MSG_RS13BUTTON;
      else if (i == 14)
        return MSG_RS14BUTTON;
      else if (i == 15)
        return MSG_RS15BUTTON;
      else if (i == 16)
        return MSG_RS16BUTTON;
      else
      {
        LOG(ALLB_LOG,LOG_ERR, "RS16 Button Set error %s to %d, %d is out of scope\n", aqdata->aqbuttons[i].label, aqdata->aqbuttons[i].led->state, i);
        return false;
      }
      
    }
  }

  return false;
}


void _processMessage(char *message, struct aqualinkdata *aqdata, bool reset);

void processMessage(char *message, struct aqualinkdata *aqdata)
{
  _processMessage(message, aqdata, false);
}
void processMessageReset(struct aqualinkdata *aqdata)
{
  _processMessage(NULL, aqdata, true);
}
void _processMessage(char *message, struct aqualinkdata *aqdata, bool reset)
{
  char *msg;
  static bool _initWithRS = false;
  //static bool _gotREV = false;
  //static int freeze_msg_count = 0;
  //static int service_msg_count = 0;
  //static int swg_msg_count = 0;
  //static int boost_msg_count = 0;
  static int16_t msg_loop = 0;
  static aqledstate default_frz_protect_state = OFF;
  static bool boostInLastLoop = false;
  // NSF replace message with msg

  int16_t rs16;


  //msg = stripwhitespace(message);
  //strcpy(aqdata->last_message, msg);
  //LOG(ALLB_LOG,LOG_INFO, "RS Message :- '%s'\n", msg);

  

  // Check long messages in this if/elseif block first, as some messages are similar.
  // ie "POOL TEMP" and "POOL TEMP IS SET TO"  so want correct match first.
  //

  //if (stristr(msg, "JANDY AquaLinkRS") != NULL) {
  if (!reset) {
    msg = stripwhitespace(message);
    strcpy(aqdata->last_message, msg);
    LOG(ALLB_LOG,LOG_INFO, "RS Message :- '%s'\n", msg);
    // Just set this to off, it will re-set since it'll be the only message we get if on
    aqdata->service_mode_state = OFF;
  } else {
    //aqdata->display_message = NULL;
    //aqdata->last_display_message[0] = ' ';
    //aqdata->last_display_message[1] = '\0';
    SET_IF_CHANGED_STRCPY(aqdata->last_display_message, "", aqdata->is_dirty);

    // Anything that wasn't on during the last set of messages, turn off
    if ((msg_loop & MSG_FREEZE) != MSG_FREEZE) {
      if (aqdata->frz_protect_state != default_frz_protect_state) {
        LOG(ALLB_LOG,LOG_INFO, "Freeze protect turned off\n");
        event_happened_set_device_state(AQS_FRZ_PROTECT_OFF, aqdata);
        // Add code to check Pump if to turn it on (was scheduled) ie time now is inbetween ON / OFF schedule 
      }
      SET_IF_CHANGED(aqdata->frz_protect_state, default_frz_protect_state, aqdata->is_dirty);
    }

    if ((msg_loop & MSG_SERVICE) != MSG_SERVICE &&
        (msg_loop & MSG_TIMEOUT) != MSG_TIMEOUT ) {
      aqdata->service_mode_state = OFF; // IF we get this message then Service / Timeout is off
    }

    if ( ((msg_loop & MSG_SWG_DEVICE) != MSG_SWG_DEVICE) && aqdata->swg_led_state != LED_S_UNKNOWN) {
      // No Additional SWG devices messages like "no flow"
      if ((msg_loop & MSG_SWG) != MSG_SWG && aqdata->aqbuttons[PUMP_INDEX].led->state == OFF )
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_OFF);
      else
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_ON);
    }

    // If no AQUAPURE message, either (no SWG, it's set 0, or it's off).
    if ((msg_loop & MSG_SWG) != MSG_SWG && aqdata->swg_led_state != LED_S_UNKNOWN ) {
      if (aqdata->swg_percent != 0 || aqdata->swg_led_state == ON) {
        // Something is wrong here.  Let's check pump, if on set SWG to 0, if off turn SWE off
        if ( aqdata->aqbuttons[PUMP_INDEX].led->state == OFF) {
          LOG(ALLB_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is off so setting SWG to off\n");
          setSWGoff(aqdata);
        } else {
          LOG(ALLB_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is on so setting SWG to 0%%\n");
          changeSWGpercent(aqdata, 0);
        }
      } else if (isIAQT_ENABLED == false && isONET_ENABLED == false && READ_RSDEV_SWG == false ) {
        //We have no other way to read SWG %=0, so turn SWG on with pump
        if ( aqdata->aqbuttons[PUMP_INDEX].led->state == ON) {
           LOG(ALLB_LOG,LOG_INFO, "No AQUAPURE message in cycle, pump is off so setting SWG to off\n");
           //changeSWGpercent(aqdata, 0);
           setSWGenabled(aqdata);  
        }
      }
        // NSF Need something to catch startup when SWG=0 so we set it to enabeled.
        // when other ways/protocols to detect SWG=0 are turned off.
    }

    if ((msg_loop & MSG_LOOP_POOL_TEMP) != MSG_LOOP_POOL_TEMP && aqdata->pool_temp != TEMP_UNKNOWN ) {
      SET_IF_CHANGED(aqdata->pool_temp, TEMP_UNKNOWN, aqdata->is_dirty);
    }
    if ((msg_loop & MSG_LOOP_SPA_TEMP) != MSG_LOOP_SPA_TEMP && aqdata->spa_temp != TEMP_UNKNOWN ) {
      SET_IF_CHANGED(aqdata->spa_temp, TEMP_UNKNOWN, aqdata->is_dirty);
    }

    /*
    //  AQUAPURE=0 we never get that message on ALLBUTTON so don't turn off unless filter pump if off
    if ((msg_loop & MSG_SWG) != MSG_SWG && aqdata->aqbuttons[PUMP_INDEX].led->state == OFF ) {
       //aqdata->ar_swg_status = SWG_STATUS_OFF;
       setSWGoff(aqdata);
    }
    */
    if ((msg_loop & MSG_BOOST) != MSG_BOOST) {
      if (aqdata->boost == true || boostInLastLoop == true) {
        LOG(ALLB_LOG,LOG_INFO, "Boost turned off\n");
        event_happened_set_device_state(AQS_BOOST_OFF, aqdata);
        // Add code to check Pump if to turn it on (was scheduled) ie time now is inbetween ON / OFF schedule
      }
      SET_IF_CHANGED(aqdata->boost, false, aqdata->is_dirty);
      aqdata->boost_msg[0] = '\0';
      SET_IF_CHANGED(aqdata->boost_duration, 0, aqdata->is_dirty);
      boostInLastLoop = false;
      //if (aqdata->swg_percent >= 101)
      //  aqdata->swg_percent = 0;
    }

    if ((msg_loop & MSG_BATTERY_LOW) != MSG_BATTERY_LOW)
      SET_IF_CHANGED(aqdata->battery, OK, aqdata->is_dirty);


    //if ( _aqconfig_.rs_panel_size >= 16) {
    //if ( (int)PANEL_SIZE >= 16) { // NSF No idea why this fails on RS-4, but it does.  Come back and find out why
    if ( PANEL_SIZE() >= 16 ) {
      //printf("Panel size %d What the fuck am I doing here\n",PANEL_SIZE());
      if ((msg_loop & MSG_RS13BUTTON) != MSG_RS13BUTTON)
        SET_IF_CHANGED(aqdata->aqbuttons[13].led->state, OFF, aqdata->is_dirty);
      if ((msg_loop & MSG_RS14BUTTON) != MSG_RS14BUTTON)
        SET_IF_CHANGED(aqdata->aqbuttons[14].led->state, OFF, aqdata->is_dirty);
      if ((msg_loop & MSG_RS15BUTTON) != MSG_RS15BUTTON)
        SET_IF_CHANGED(aqdata->aqbuttons[15].led->state, OFF, aqdata->is_dirty);
      if ((msg_loop & MSG_RS16BUTTON) != MSG_RS16BUTTON)
        SET_IF_CHANGED(aqdata->aqbuttons[16].led->state, OFF, aqdata->is_dirty);
    }

    msg_loop = 0;
    return;
  }

  if (stristr(msg, LNG_MSG_BATTERY_LOW) != NULL)
  {
    SET_IF_CHANGED(aqdata->battery, LOW, aqdata->is_dirty);
    msg_loop |= MSG_BATTERY_LOW;
    //strcpy(aqdata->last_display_message, msg); // Also display the message on web UI
    SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);
  }
  else if (stristr(msg, LNG_MSG_POOL_TEMP_SET) != NULL)
  {
    //LOG(ALLB_LOG,LOG_DEBUG, "**************** pool htr long message: %s", &message[20]);
    SET_IF_CHANGED(aqdata->pool_htr_set_point, atoi(message + 20), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  else if (stristr(msg, LNG_MSG_SPA_TEMP_SET) != NULL)
  {
    //LOG(ALLB_LOG,LOG_DEBUG, "spa htr long message: %s", &message[19]);
    SET_IF_CHANGED(aqdata->spa_htr_set_point, atoi(message + 19), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_SET) != NULL)
  {
    //LOG(ALLB_LOG,LOG_DEBUG, "frz protect long message: %s", &message[28]);
    SET_IF_CHANGED(aqdata->frz_protect_set_point, atoi(message + 28), aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->frz_protect_state, ENABLE, aqdata->is_dirty);
    default_frz_protect_state = ENABLE;

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  else if (strncasecmp(msg, MSG_AIR_TEMP, MSG_AIR_TEMP_LEN) == 0)
  {
    SET_IF_CHANGED(aqdata->air_temp, atoi(msg + MSG_AIR_TEMP_LEN), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  else if (strncasecmp(msg, MSG_POOL_TEMP, MSG_POOL_TEMP_LEN) == 0)
  {
    msg_loop |= MSG_LOOP_POOL_TEMP;
    SET_IF_CHANGED(aqdata->pool_temp, atoi(msg + MSG_POOL_TEMP_LEN), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  else if (strncasecmp(msg, MSG_SPA_TEMP, MSG_SPA_TEMP_LEN) == 0)
  {
    msg_loop |= MSG_LOOP_SPA_TEMP;
    SET_IF_CHANGED(aqdata->spa_temp, atoi(msg + MSG_SPA_TEMP_LEN), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);
  }
  // NSF If get water temp rather than pool or spa in some cases, then we are in Pool OR Spa ONLY mode
  else if (strncasecmp(msg, MSG_WATER_TEMP, MSG_WATER_TEMP_LEN) == 0)
  {
    SET_IF_CHANGED(aqdata->pool_temp, atoi(msg + MSG_WATER_TEMP_LEN), aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->spa_temp, atoi(msg + MSG_WATER_TEMP_LEN), aqdata->is_dirty);
    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(ALLB_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP1_SET) != NULL)
  {
    SET_IF_CHANGED(aqdata->pool_htr_set_point, atoi(message + 28), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(ALLB_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_WATER_TEMP2_SET) != NULL)
  {
    SET_IF_CHANGED(aqdata->spa_htr_set_point, atoi(message + 27), aqdata->is_dirty);

    if (aqdata->temp_units == UNKNOWN)
      setUnits(msg, aqdata);

    if (isSINGLE_DEV_PANEL != true)
    {
      changePanelToMode_Only();
      LOG(ALLB_LOG,LOG_ERR, "AqualinkD set to 'Combo Pool & Spa' but detected 'Only Pool OR Spa' panel, please change config\n");
    }
  }
  else if (stristr(msg, LNG_MSG_SERVICE_ACTIVE) != NULL)
  {
    if (aqdata->service_mode_state == OFF)
      LOG(ALLB_LOG,LOG_NOTICE, "AqualinkD set to Service Mode\n");
    SET_IF_CHANGED(aqdata->service_mode_state, ON, aqdata->is_dirty);
     msg_loop |= MSG_SERVICE;
    //service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_TIMEOUT_ACTIVE) != NULL)
  {
    if (aqdata->service_mode_state == OFF)
      LOG(ALLB_LOG,LOG_NOTICE, "AqualinkD set to Timeout Mode\n");
    SET_IF_CHANGED(aqdata->service_mode_state, FLASH, aqdata->is_dirty);
     msg_loop |= MSG_TIMEOUT;
    //service_msg_count = 0;
  }
  else if (stristr(msg, LNG_MSG_FREEZE_PROTECTION_ACTIVATED) != NULL)
  {
    msg_loop |= MSG_FREEZE;
    //aqdata->frz_protect_state = default_frz_protect_state;
    SET_IF_CHANGED(aqdata->frz_protect_state, ON, aqdata->is_dirty);
    //freeze_msg_count = 0;
    //strcpy(aqdata->last_display_message, msg); // Also display the message on web UI
    SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);
  }
  
  else if (ENABLE_CHILLER && (stristr(msg,"Chiller") != NULL || stristr(msg,"Heat Pump") != NULL)) {
    processHeatPumpDisplayMessage(msg, aqdata); // This doesn;t exist yet
  }
  
  /* // Not sure when to do with these for the moment, so no need to compile in the test.
  else if (stristr(msg, LNG_MSG_CHEM_FEED_ON) != NULL) {
  }
  else if (stristr(msg, LNG_MSG_CHEM_FEED_OFF) != NULL) {
  }
  */
  else if (msg[2] == '/' && msg[5] == '/' && msg[8] == ' ')
  { // date in format '08/29/16 MON'
    //strcpy(aqdata->date, msg);
    SET_IF_CHANGED_STRCPY(aqdata->date, msg, aqdata->is_dirty);
  }
  else if (stristr(msg, MSG_SWG_PCT) != NULL) 
  {
    if (strncasecmp(msg, MSG_SWG_PCT, MSG_SWG_PCT_LEN) == 0 && strncasecmp(msg, "AQUAPURE HRS", 12) != 0) {
      changeSWGpercent(aqdata, atoi(msg + MSG_SWG_PCT_LEN));
    } 
    else if (strncasecmp(msg, "AQUAPURE HRS", 12) != 0 && strncasecmp(msg, "SET AQUAPURE", 12) != 0) 
    {
      if (strcasestr(msg, MSG_SWG_NO_FLOW) != NULL)
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_NO_FLOW);
      else if (strcasestr(msg, MSG_SWG_LOW_SALT) != NULL)
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_LOW_SALT);
      else if (strcasestr(msg, MSG_SWG_HIGH_SALT) != NULL)
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_HI_SALT);
      else if (strcasestr(msg, MSG_SWG_FAULT) != NULL)
        setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_GENFAULT);
        //setSWGdeviceStatus(aqdata, ALLBUTTON, SWG_STATUS_CHECK_PCB);
      
      // Any of these messages want to display.
      //strcpy(aqdata->last_display_message, msg);
      SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);

      msg_loop |= MSG_SWG_DEVICE;
    }
    msg_loop |= MSG_SWG;
  }
  else if (strncasecmp(msg, MSG_SWG_PPM, MSG_SWG_PPM_LEN) == 0)
  {
    SET_IF_CHANGED( aqdata->swg_ppm, atoi(msg + MSG_SWG_PPM_LEN), aqdata->is_dirty);
    msg_loop |= MSG_SWG;
  }
  else if ((msg[1] == ':' || msg[2] == ':') && msg[strlen(msg) - 1] == 'M')
  { // time in format '9:45 AM'
    //strcpy(aqdata->time, msg);
    SET_IF_CHANGED_STRCPY(aqdata->time, msg, aqdata->is_dirty);
    // Setting time takes a long time, so don't try until we have all other programmed data.
    if (_initWithRS == true && strlen(aqdata->date) > 1 && checkAqualinkTime() != true)
    {
      LOG(ALLB_LOG,LOG_NOTICE, "Time is NOT accurate '%s %s', re-setting on controller!\n", aqdata->time, aqdata->date);
      aq_programmer(AQ_SET_TIME, NULL, aqdata);
    }
    else if (_initWithRS == false || _aqconfig_.sync_panel_time == false)
    {
      LOG(ALLB_LOG,LOG_DEBUG, "RS time '%s %s' not checking\n", aqdata->time, aqdata->date);
    }
    else if (_initWithRS == true)
    {
      LOG(ALLB_LOG,LOG_DEBUG, "RS time is accurate '%s %s'\n", aqdata->time, aqdata->date);
    }
    // If we get a time message before REV, the controller didn't see us as we started too quickly.
    /* Don't need to check this anymore with the check for probe before startup.
    if (_gotREV == false)
    {
      LOG(ALLB_LOG,LOG_NOTICE, "Getting control panel information\n", msg);
      aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, aqdata);
      _gotREV = true; // Force it to true just incase we don't understand the model#
    }
    */
  }
  else if (strstr(msg, " REV ") != NULL || strstr(msg, " REV. ") != NULL)
  { // '8157 REV MMM'
    // A master firmware revision message.
    //strcpy(aqdata->version, msg);
    SET_IF_CHANGED_STRCPY(aqdata->version, msg, aqdata->is_dirty);
    rsm_get_revision(aqdata->revision, aqdata->version, strlen(aqdata->version));
    setPanelInformationFromPanelMsg(aqdata, msg, PANEL_CPU | PANEL_REV, ALLBUTTON);
    //setBoardCPURevision(aqdata, aqdata->version, strlen(aqdata->version), ALLB_LOG);
    //_gotREV = true;
    LOG(ALLB_LOG,LOG_DEBUG, "Control Panel version %s\n", aqdata->version);
    LOG(ALLB_LOG,LOG_DEBUG, "Control Panel revision %s\n", aqdata->revision);
    if (_initWithRS == false)
    {
      //LOG(ALLBUTTON,LOG_NOTICE, "Standard protocol initialization complete\n");
      queueGetProgramData(ALLBUTTON, aqdata);
      event_happened_set_device_state(AQS_POWER_ON, aqdata);
      //queueGetExtendedProgramData(ALLBUTTON, aqdata, _aqconfig_.use_panel_aux_labels);
      _initWithRS = true;
    }
  }
  else if (stristr(msg, " TURNS ON") != NULL)
  {
    LOG(ALLB_LOG,LOG_NOTICE, "Program data '%s'\n", msg);
  }
  else if (_aqconfig_.override_freeze_protect == TRUE && strncasecmp(msg, "Press Enter* to override Freeze Protection with", 47) == 0)
  {
    //send_cmd(KEY_ENTER, aqdata);
    //aq_programmer(AQ_SEND_CMD, (char *)KEY_ENTER, aqdata);
    aq_send_allb_cmd(KEY_ENTER);
  }
  // Process any button states (fake LED) for RS12 and above keypads
  // Text will be button label on or off  ie Aux_B2 off or WaterFall off
  

  //else if ( _aqconfig_.rs_panel_size >= 16 && (rs16 = RS16_endswithLEDstate(msg)) != 0 )
  else if (PANEL_SIZE() >= 16 && (rs16 = RS16_endswithLEDstate(msg, aqdata)) != 0 )
  {
    msg_loop |= rs16;
    // Do nothing, just stop other else if statments executing
    // make sure we also display the message.
    // Note we only get ON messages here, Off messages will not be sent if something else turned it off
    // use the Onetouch or iAqua equiptment page for off.
    //strcpy(aqdata->last_display_message, msg);
    SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);
  }

  else if (((msg[4] == ':') || (msg[6] == ':')) && (strncasecmp(msg, "AUX", 3) == 0) )
  { // Should probable check we are in programming mode.
    // 'Aux3: No Label'
    // 'Aux B1: No Label'
    int labelid;
    int ni = 3;
    if (msg[4] == 'B') { ni = 5; }
    labelid = atoi(msg + ni);
    if (labelid > 0 && _aqconfig_.use_panel_aux_labels == true)
    {
      if (ni == 5)
        labelid = labelid + 8;
      else
        labelid = labelid + 1;
      // Aux1: on panel = Button 3 in aqualinkd  (button 2 in array)
      if (strncasecmp(msg+ni+3, "No Label", 8) != 0) {
        aqdata->aqbuttons[labelid].label = prittyString(cleanalloc(msg+ni+2));
        LOG(ALLB_LOG,LOG_NOTICE, "AUX ID %s label set to '%s'\n", aqdata->aqbuttons[labelid].name, aqdata->aqbuttons[labelid].label);
      } else {
        LOG(ALLB_LOG,LOG_NOTICE, "AUX ID %s has no control panel label using '%s'\n", aqdata->aqbuttons[labelid].name, aqdata->aqbuttons[labelid].label);
      }
      //aqdata->aqbuttons[labelid + 1].label = cleanalloc(msg + 5);
    }
  }
  // BOOST POOL 23:59 REMAINING
  else if ( (strncasecmp(msg, "BOOST POOL", 10) == 0) && (strcasestr(msg, "REMAINING") != NULL) ) {
    // Ignore messages if in programming mode.  We get one of these turning off for some strange reason.
    if (in_programming_mode(aqdata) == false) {

      if (aqdata->boost == false || boostInLastLoop == false) {
        event_happened_set_device_state(AQS_BOOST_ON, aqdata);
      }

      snprintf(aqdata->boost_msg, 6, "%s", &msg[11]);
      aqdata->boost_duration = rsm_HHMM2min(aqdata->boost_msg);
      SET_IF_CHANGED(aqdata->boost, true, aqdata->is_dirty);
      msg_loop |= MSG_BOOST;
      msg_loop |= MSG_SWG;
      boostInLastLoop = true;
      //convert_boost_to_duration(aqdata->boost_msg)
      //if (aqdata->ar_swg_status != SWG_STATUS_ON) {aqdata->ar_swg_status = SWG_STATUS_ON;}
      if (aqdata->swg_percent != 101) {changeSWGpercent(aqdata, 101);}
      //boost_msg_count = 0;
    //if (aqdata->active_thread.thread_id == 0)
      //strcpy(aqdata->last_display_message, msg); // Also display the message on web UI if not in programming mode
      SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);
    }
  }
  else
  {
    LOG(ALLB_LOG,LOG_DEBUG_SERIAL, "Ignoring '%s'\n", msg);
    //aqdata->display_message = msg;
    //if (in_programming_mode(aqdata) == false && aqdata->simulate_panel == false &&
    if (in_programming_mode(aqdata) == false &&
        stristr(msg, "JANDY AquaLinkRS") == NULL &&
        //stristr(msg, "PUMP O") == NULL  &&// Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        strncasecmp(msg, "PUMP O", 6) != 0  &&// Catch 'PUMP ON' and 'PUMP OFF' but not 'PUMP WILL TURN ON'
        stristr(msg, "MAINTAIN") == NULL && // Catch 'MAINTAIN TEMP IS OFF'
        stristr(msg, "Heat Pump") == NULL && // Stop Heatpump on / off messages spam the UI.
        stristr(msg, "0 PSI") == NULL /* // Catch some erronious message on test harness
        stristr(msg, "CLEANER O") == NULL &&
        stristr(msg, "SPA O") == NULL &&
        stristr(msg, "AUX") == NULL*/
    )
    { // Catch all AUX1 AUX5 messages
      //aqdata->display_last_message = true;
      //strcpy(aqdata->last_display_message, msg);
      SET_IF_CHANGED_STRCPY(aqdata->last_display_message, msg, aqdata->is_dirty);
      //rsm_strncpy(aqdata->last_display_message, (unsigned char *)msg, AQ_MSGLONGLEN, AQ_MSGLONGLEN);
    }
  }

  // Send every message if we are in simulate panel mode
  //if (aqdata->simulate_panel)
  //  strcpy(aqdata->last_display_message, msg);
    //rsm_strncpy(aqdata->last_display_message, (unsigned char *)msg, AQ_MSGLONGLEN, AQ_MSGLONGLEN);
    //ascii(aqdata->last_display_message, msg);


  //LOG(ALLB_LOG,LOG_INFO, "RS Message loop :- '%d'\n", msg_loop);

  // We processed the next message, kick any threads waiting on the message.
//printf ("Message kicking\n");


  kick_aq_program_thread(aqdata, ALLBUTTON);
}

bool process_allbutton_packet(unsigned char *packet, int length, struct aqualinkdata *aqdata)
{
  bool rtn = false;
  //static unsigned char last_packet[AQ_MAXPKTLEN];
  static unsigned char last_checksum;
  static char message[AQ_MSGLONGLEN + 1];
  static int processing_long_msg = 0;

  // Check packet against last check if different.
  // Should only use the checksum, not whole packet since it's status messages.
  /*
  if ( packet[PKT_CMD] == CMD_STATUS && (memcmp(packet, last_packet, length) == 0))
  {
    LOG(ALLB_LOG,LOG_DEBUG_SERIAL, "RS Received duplicate, ignoring.\n", length);
    return rtn;
  }
  else
  {
    memcpy(last_packet, packet, length);
   aqdata->last_packet_type = packet[PKT_CMD];
    rtn = true;
  }
  */

  aqdata->last_packet_type = packet[PKT_CMD];


  if ( packet[PKT_CMD] == CMD_STATUS && packet[length-3] == last_checksum && ! in_programming_mode(aqdata) )
  {
    LOG(ALLB_LOG,LOG_DEBUG_SERIAL, "RS Received duplicate, ignoring.\n", length);
    return false;
  }
  else
  {
    last_checksum = packet[length-3];
    rtn = true;
  }

  if (processing_long_msg > 0 && packet[PKT_CMD] != CMD_MSG_LONG)
  {
    processing_long_msg = 0;
    //LOG(ALLB_LOG,LOG_ERR, "RS failed to receive complete long message, received '%s'\n",message);
    //LOG(ALLB_LOG,LOG_DEBUG, "RS didn't finished receiving of MSG_LONG '%s'\n",message);
    processMessage(message, aqdata);
  }

  LOG(ALLB_LOG,LOG_DEBUG_SERIAL, "RS Received packet type 0x%02hhx length %d.\n", packet[PKT_CMD], length);

  switch (packet[PKT_CMD])
  {
  case CMD_ACK:
    //LOG(ALLB_LOG,LOG_DEBUG_SERIAL, "RS Received ACK length %d.\n", length);
    break;
  case CMD_STATUS:
    //LOG(ALLB_LOG,LOG_DEBUG, "RS Received STATUS length %d.\n", length);
    //debuglogPacket(ALLB_LOG, packet, length, true, true);
    
    //memcpy(aqdata->raw_status, packet + 4, AQ_PSTLEN);
    //processLEDstate(aqdata);
    processLEDstate(aqdata, packet, ALLB_LOG);

    /* NSF Take this out, and use the ALLButton loop cycle to determin if we get spa/pool temp
       messages.  Works better for dual equiptment when both pool & spa pumps and dual temp sensors */
    /*
    if (aqdata->aqbuttons[PUMP_INDEX].led->state == OFF)
    {
     aqdata->pool_temp = TEMP_UNKNOWN;
     aqdata->spa_temp = TEMP_UNKNOWN;
      //aqdata->spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
    }
    else if (aqdata->aqbuttons[SPA_INDEX].led->state == OFF && isSINGLE_DEV_PANEL != true)
    {
      //aqdata->spa_temp = _aqconfig_.report_zero_spa_temp?-18:TEMP_UNKNOWN;
     aqdata->spa_temp = TEMP_UNKNOWN;
    }
    else if (aqdata->aqbuttons[SPA_INDEX].led->state == ON && isSINGLE_DEV_PANEL != true)
    {
     aqdata->pool_temp = TEMP_UNKNOWN;
    }
    */

    // COLOR MODE programming relies on state changes, so let any threads know
    //if (aqdata->active_thread.ptype == AQ_SET_LIGHTPROGRAM_MODE) {
    if ( in_light_programming_mode(aqdata) ) {
      kick_aq_program_thread(aqdata, ALLBUTTON);
    }
    break;
  case CMD_MSG:
  case CMD_MSG_LONG:
     {
       int index = packet[PKT_DATA]; // Will get 0x00 for complete message, 0x01 for start on long message 0x05 last of long message
       //printf("RSM received message at index %d '%.*s'\n",index,AQ_MSGLEN,(char *)packet + PKT_DATA + 1);
       if (index <= 1){
         memset(message, 0, AQ_MSGLONGLEN + 1);
         //strncpy(message, (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         rsm_strncpy(message, packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         processing_long_msg = index;
         //LOG(ALLB_LOG,LOG_ERR, "Message %s\n",message);
       } else {
         //strncpy(&message[(processing_long_msg * AQ_MSGLEN)], (char *)packet + PKT_DATA + 1, AQ_MSGLEN);
         //rsm_strncpy(&message[(processing_long_msg * AQ_MSGLEN)], (unsigned char *)packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         rsm_strncpy(&message[( (index-1) * AQ_MSGLEN)], (unsigned char *)packet + PKT_DATA + 1, AQ_MSGLONGLEN, AQ_MSGLEN);
         //LOG(ALLB_LOG,LOG_ERR, "Long Message %s\n",message);
         if (++processing_long_msg != index) {
           LOG(ALLB_LOG,LOG_DEBUG, "Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
           //printf("RSM Long message index %d doesn't match buffer %d\n",index,processing_long_msg);
         }
         #ifdef  PROCESS_INCOMPLETE_MESSAGES
           kick_aq_program_thread(aqdata, ALLBUTTON);
         #endif
       }

       if (index == 0 || index == 5) {
         //printf("RSM process message '%s'\n",message);
         
         // MOVED FROM LINE 701 see if less errors
         //kick_aq_program_thread(aqdata, ALLBUTTON);

         LOG(ALLB_LOG,LOG_DEBUG, "Processing Message - '%s'\n",message);
         processMessage(message, aqdata); // This will kick thread
       }
       
     }
    break;
  case CMD_PROBE:
    LOG(ALLB_LOG,LOG_DEBUG, "RS Received PROBE length %d.\n", length);
    //LOG(ALLB_LOG,LOG_INFO, "Synch'ing with Aqualink master device...\n");
    rtn = false;
    break;
  case CMD_MSG_LOOP_ST:
    LOG(ALLB_LOG,LOG_INFO, "RS Received message loop start\n");
    processMessageReset(aqdata);
    rtn = false;
    break;
  default:
    LOG(ALLB_LOG,LOG_INFO, "RS Received unknown packet, 0x%02hhx\n", packet[PKT_CMD]);
    rtn = false;
    break;
  }

  return rtn;
}