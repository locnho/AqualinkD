#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rs_devices.h"
#include "aqualink.h"
#include "aq_serial.h"
#include "auto_configure.h"
#include "utils.h"
#include "config.h"
#include "rs_msg_utils.h"


#define MAX_AUTO_PACKETS 1200


char *_getPanelInfoAllB(int rs_fd, unsigned char *packet_buffer)
{
  static unsigned char allbID = 0x00;
  static bool found=false;

  if (found)
    return NULL;

  if (packet_buffer[PKT_CMD] == CMD_PROBE) {
    if (allbID == 0x00) {
      allbID = packet_buffer[PKT_DEST];
      send_ack(rs_fd, 0x00);
    }
  } else if ( allbID == packet_buffer[PKT_DEST] ) {
    if ( packet_buffer[PKT_CMD] == CMD_MSG ) {
      char *ptr;
      if ( (ptr = rsm_strnstr((char *)&packet_buffer[5], " REV", AQ_MSGLEN)) != NULL ) {
        found=true;
        return ptr+1;
      }
    }
    send_ack(rs_fd, 0x00);
  }

  return NULL;
}

void *_getPanelInfoPCdock(int rs_fd, unsigned char *packet_buffer)
{
  static unsigned char getPanelRev[] = {0x00,0x14,0x01};
  static unsigned char getPanelType[] = {0x00,0x14,0x02};
  static int msgcnt=0;
  static int found=0;
  static unsigned char pcdID = 0x00;

  if (found >= 2)
    return NULL;

  if (packet_buffer[PKT_CMD] == CMD_PROBE) {
    if (msgcnt == 0) {
      pcdID = packet_buffer[PKT_DEST];
      send_ack(rs_fd, 0x00);
    } else if (msgcnt == 1 && pcdID == packet_buffer[PKT_DEST])
      send_jandy_command(rs_fd, getPanelRev, 3);
    else if (msgcnt == 2 && pcdID == packet_buffer[PKT_DEST])
      send_jandy_command(rs_fd, getPanelType, 3);
    msgcnt++;
  } else if (packet_buffer[PKT_CMD] == CMD_MSG && pcdID == packet_buffer[PKT_DEST]) {
    send_ack(rs_fd, 0x00);
    if (msgcnt == 2) {
      found++;
      return (char *)&packet_buffer[5];
    } else if (msgcnt == 3) {
      found++;
      return (char *)&packet_buffer[5];
    }
  }

  return NULL;

}


bool auto_configure(struct aqualinkdata *aqdata, unsigned char* packet, int packet_length, int rs_fd) {
  // Loop over PROBE packets and store any we can use,
  // once we see the 2nd probe of any ID we fave stored, then the loop is complete, 
  // set ID's and exit true, exit falce to get called again.
/*
  unsigned char _goodID[] = {0x0a, 0x0b, 0x08, 0x09};
  unsigned char _goodPDAID[] = {0x60, 0x61, 0x62, 0x63}; // PDA Panel only supports one PDA.
  unsigned char _goodONETID[] = {0x40, 0x41, 0x42, 0x43};
  unsigned char _goodIAQTID[] = {0x30, 0x31, 0x32, 0x33};
  unsigned char _goodRSSAID[] = {0x48, 0x49};  // Know there are only 2 good RS SA id's, guess 0x49 is the second.
*/

  static unsigned char firstprobe = 0x00;
  static unsigned char lastID = 0x00;
  static unsigned char PDA_ID = 0x00;
  static bool seen_iAqualink2 = false;
  static int foundIDs = 0;
  static int packetsReceived=0;
  static bool done=false;
  static bool gotRev = false;
  static bool gotPsize = false;
  static int loopsCompleted=0;
  static unsigned char passMessageAllB = 0xFF;
  static unsigned char passMessagePCdock = 0xFF;

  char *msg_ptr;

  if (++packetsReceived >= MAX_AUTO_PACKETS ) {
    LOG(AQUA_LOG,LOG_ERR, "Received %d packets, and didn't get a full probe cycle, stoping Auto Configure!\n",packetsReceived);
    //return true;
    done=true;
    goto checkIDs;
  }

  if (packet[PKT_DEST] == passMessageAllB && gotRev == false) {
    if ( (msg_ptr = _getPanelInfoAllB(rs_fd, packet)) != NULL) {
      uint8_t sets = setPanelInformationFromPanelMsg(aqdata, msg_ptr, PANEL_CPU | PANEL_REV, SIM_NONE);
      if (isMASK_SET(sets, PANEL_REV))
        gotRev = true;
      return false;
    }
  } else if (packet[PKT_DEST] == passMessagePCdock && (gotRev == false || gotPsize == false) ) {
    if ( (msg_ptr = _getPanelInfoPCdock(rs_fd, packet)) != NULL) {
      uint8_t sets = setPanelInformationFromPanelMsg(aqdata, msg_ptr,  PANEL_REV | PANEL_STRING, SIM_NONE);
      if (isMASK_SET(sets, PANEL_REV))
        gotRev = true;
      if (isMASK_SET(sets, PANEL_STRING))
        gotPsize = true;
      return false;
    }
  } else if ( packet[PKT_CMD] == CMD_PROBE ) {
    if ( packet[PKT_DEST] >= 0x08 && packet[PKT_DEST] <= 0x0B && gotRev == false && passMessageAllB == 0xFF) {
      _getPanelInfoAllB(rs_fd, packet);
      passMessageAllB = packet[PKT_DEST];
      LOG(AQUA_LOG, LOG_NOTICE, "Using id=0x%02hhx to probe panel for information\n",packet[PKT_DEST]);
      return false;
    } else if ( packet[PKT_DEST] == 0x58 && (gotRev == false || gotPsize == false) && passMessagePCdock == 0xFF) {
      _getPanelInfoPCdock(rs_fd, packet);
      passMessagePCdock = packet[PKT_DEST];
      LOG(AQUA_LOG, LOG_NOTICE, "Using id=0x%02hhx to probe panel for information\n",packet[PKT_DEST]);
      return false;
    }
  }

  // PDA might be active, so capture PDA ID ignoring if it's free or not.
  if ( (packet[PKT_DEST] >= 0x60 && packet[PKT_DEST] <= 0x63) && PDA_ID == 0x00) {
    LOG(AQUA_LOG,LOG_NOTICE, "Found valid PDA ID 0x%02hhx\n",packet[PKT_DEST]);
    PDA_ID = packet[PKT_DEST];
  }

  if (is_iaqualink_id(lastID) && packet[PKT_DEST] == DEV_MASTER && seen_iAqualink2 == false ) 
  { // Saw a iAqualink2/3 device, so can't use ID, but set to read device info.
    // NSF This is not a good way to check, will probably be false positive if you are using iAqualink2 and hit restart.
    _aqconfig_.extended_device_id2 = 0x00;
    _aqconfig_.enable_iaqualink = false;
    _aqconfig_.read_RS485_devmask |= READ_RS485_IAQUALNK;
    seen_iAqualink2 = true;
    LOG(AQUA_LOG,LOG_NOTICE, "Saw inuse iAqualink2/3 ID 0x%02hhx, turning off AqualinkD on that ID\n",lastID);
  } 



  if (lastID != 0x00 && packet[PKT_DEST] == DEV_MASTER ) { // Can't use got a reply to the last probe.
    lastID = 0x00;
  }
  else if (lastID != 0x00 && packet[PKT_DEST] != DEV_MASTER) 
  {
    // We can use last ID.
    // Save the first good ID.
    if (firstprobe == 0x00 && lastID != 0x60) {
      // NOTE IF can't use 0x60 (or PDA ID's) for probe, as they are way too often.
      //printf("*** First Probe 0x%02hhx\n",lastID);
      firstprobe = lastID;
      _aqconfig_.device_id = 0x00;
      _aqconfig_.rssa_device_id = 0x00;
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
      //AddAQDstatusMask(AUTOCONFIGURE_ID);
      setMASK(aqdata->status_mask, AUTOCONFIGURE_ID);
      SET_DIRTY(aqdata->is_dirty);
      //AddAQDstatusMask(AUTOCONFIGURE_PANEL); // Not implimented yet.
    }
    if ( is_allbutton_id(lastID) && (_aqconfig_.device_id == 0x00 || _aqconfig_.device_id == 0xFF) ) {
      _aqconfig_.device_id = lastID;
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused device ID 0x%02hhx\n",lastID);
      foundIDs++;
    } else if ( is_rsserialadapter_id(lastID) && (_aqconfig_.rssa_device_id == 0x00 || _aqconfig_.rssa_device_id == 0xFF) ) {
      _aqconfig_.rssa_device_id = lastID;
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused RSSA ID 0x%02hhx\n",lastID);
      foundIDs++;
    } else if ( is_onetouch_id(lastID) && (_aqconfig_.extended_device_id == 0x00 || _aqconfig_.extended_device_id == 0xFF) ) {
      _aqconfig_.extended_device_id = lastID;
      _aqconfig_.extended_device_id_programming = true;
      // Don't increase  foundIDs as we prefer not to use this one.
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused extended ID 0x%02hhx\n",lastID);
    } else if ( is_aqualink_touch_id(lastID) && (_aqconfig_.extended_device_id < 0x30 || _aqconfig_.extended_device_id > 0x33)) { //Overide if it's been set to Touch or not set.
      _aqconfig_.extended_device_id = lastID;
      _aqconfig_.extended_device_id_programming = true;
      if (!seen_iAqualink2) {
        _aqconfig_.enable_iaqualink = true;
        _aqconfig_.read_RS485_devmask &= ~ READ_RS485_IAQUALNK; // Remove this mask, as no need since we enabled iaqualink 
      }
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused extended ID 0x%02hhx\n",lastID);
      foundIDs++;
    } 
    
    // Now reset ID
    lastID = 0x00;

    return false;
  }

  if (packet[PKT_DEST] == firstprobe && packet[PKT_CMD] == CMD_PROBE) {
    loopsCompleted++;
  }

  if ( (foundIDs >= 3 && gotRev && gotPsize) || loopsCompleted >= 2 ) {
    done=true;
    goto checkIDs;
  }

  if ( (packet[PKT_CMD] == CMD_PROBE) && (
       is_allbutton_id(packet[PKT_DEST]) ||
       //(packet[PKT_DEST] >= 0x60 && packet[PKT_DEST] <= 0x63) ||
       is_rsserialadapter_id(packet[PKT_DEST]) ||
       is_aqualink_touch_id(packet[PKT_DEST]) ||
       is_onetouch_id(packet[PKT_DEST])  ))
  {
    lastID = packet[PKT_DEST]; // Store the valid ID.
  } 
  else if (is_iaqualink_id(packet[PKT_DEST])) {
    lastID = packet[PKT_DEST]; // Store the valid ID.
  }
  /*
  else if (is_iaqualink_id(packet[PKT_DEST]) && seen_iAqualink2 == false ) // we get a packet to iAqualink2/3 make sure to turn off, 
  { // Saw a iAqualink2/3 device, so can't use ID, but set to read device info.
    // NSF This is not a good way to check, will probably be false positive if you are using iAqualink2 and hit restart.
    _aqconfig_.extended_device_id2 = 0x00;
    _aqconfig_.enable_iaqualink = false;
    _aqconfig_.read_RS485_devmask |= READ_RS485_IAQUALNK;
    seen_iAqualink2 = true;
    LOG(AQUA_LOG,LOG_NOTICE, "Saw inuse iAqualink2/3 ID 0x%02hhx, turning off AqualinkD on that ID\n",packet[PKT_DEST]);
  }*/

  if (!done)
    return false;


  checkIDs:

  //printf("Total loops = %d, Found ID's = %d, using ID 0x%02hhx\n",loopsCompleted,foundIDs,firstprobe);

  if (isPDA_PANEL || (PDA_ID != 0x00 && _aqconfig_.device_id == 0x00) ) {
    LOG(AQUA_LOG,LOG_WARNING, "Autoconfigure set to PDA panel - Using most basic mode, may want to re-configure later\n");
    _aqconfig_.device_id = PDA_ID;
    _aqconfig_.extended_device_id = 0x00;
    _aqconfig_.rssa_device_id = 0x00;
    _aqconfig_.enable_iaqualink = false;
    _aqconfig_.extended_device_id_programming = false;
    if (!isPDA_PANEL) {
      _aqconfig_.paneltype_mask |= RSP_PDA;
      _aqconfig_.paneltype_mask &= ~RSP_RS;
    }
  }

  if (gotRev) {
    if ( !isMASKSET(aqdata->panel_support_options, RSP_SUP_AQLT)) {
      LOG(AQUA_LOG,LOG_NOTICE, "Ignoring AqualinkTouch probes due to panel rev\n");
      if ( _aqconfig_.extended_device_id >= 0x30 && _aqconfig_.extended_device_id <= 0x33 ) {
        _aqconfig_.extended_device_id = 0x00;
        _aqconfig_.enable_iaqualink = false;
        _aqconfig_.read_RS485_devmask &= ~ READ_RS485_IAQUALNK;
        foundIDs--;
      }
    }

    if ( !isMASKSET(aqdata->panel_support_options, RSP_SUP_ONET)) {
      LOG(AQUA_LOG,LOG_NOTICE, "Ignoring OneTouch probes due to panel rev\n");
      if ( _aqconfig_.extended_device_id >= 0x40 && _aqconfig_.extended_device_id <= 0x43 ) {
        _aqconfig_.extended_device_id = 0x00;
        foundIDs--;
      }
    }
  }

  LOG(AQUA_LOG,LOG_NOTICE, "Finished Autoconfigure using device_id=0x%02hhx rssa_device_id=0x%02hhx extended_device_id=0x%02hhx (%s iAqualink2/3)\n",
                              _aqconfig_.device_id,_aqconfig_.rssa_device_id,_aqconfig_.extended_device_id,  _aqconfig_.enable_iaqualink?"Enable":"Disable");
  removeMASK(aqdata->status_mask, AUTOCONFIGURE_ID);
  SET_DIRTY(aqdata->is_dirty);

  return true;
}