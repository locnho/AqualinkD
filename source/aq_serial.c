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
#include <stdarg.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>
// Below is needed to set low latency.
#include <linux/serial.h>





#include "aq_serial.h"
#include "utils.h"
#include "config.h"
#include "packetLogger.h"
#include "timespec_subtract.h"
#include "aqualink.h"
#include <sys/select.h>


#define SERIAL_READ_TIMEOUT_SEC 2;

static int _RS485_fds = -1;

static struct timespec _last_serial_read_time;

void send_packet(int fd, unsigned char *packet, int length);
//unsigned char getProtocolType(unsigned char* packet);

emulation_type getJandyDeviceType(unsigned char ID) {
  
  // Using emulation_type from aqprogrammer. At some point may merge into one
  // and call device type

  if (ID >= 0x08 && ID <= 0x0B)
    return ALLBUTTON;
  if (ID >= 0x40 && ID <= 0x43)
    return ONETOUCH;
  if (ID >= 0x48 && ID <= 0x4B)
    return RSSADAPTER;
  if (ID >= 0x60 && ID <= 0x63)
    return AQUAPDA;
  if (ID >= 0x30 && ID <= 0x33)
    return IAQTOUCH;
   if (ID >= 0xa0 && ID <= 0xa3)
    return IAQUALNK;

/*
  if (ID >= 0x00 && ID <= 0x03)
    return MASTER;
  if (ID >= 0x50 && ID <= 0x53)
    return SWG;
  if (ID >= 0x20 && ID <= 0x23)
    return SPA_R;
  if (ID >= 0x38 && ID <= 0x3B)
    return LX_HEATER;
  if (ID >= 0x58 && ID <= 0x5B)
    return PC_DOCK; 
  if (ID >= 0x68 && ID <= 0x6B)
    return JXI_HEATER;
  //if (ID >= 0x70 && ID <= 0x73)
  if (ID >= 0x78 && ID <= 0x7B)
    return EPUMP;
  if (ID >= 0x80 && ID <= 0x83)
    return CHEM;
  //if (ID == 0x08)
  //  return KEYPAD;
*/
  return SIM_NONE;
}

const char *getJandyDeviceName(emulation_type etype) {
  switch(etype){
    case ALLBUTTON:
      return "AllButton";
    break;
    case ONETOUCH:
      return "OneTouch";
    break;
    case RSSADAPTER:
      return "RS SerialAdapter"; 
    break;
    case IAQTOUCH:
      return "iAqualinkTouch";
    break;
    case AQUAPDA:
      return "PDA";
    break;
    case SIM_NONE:
      return "AutoConfig";
    break;
    default:
      return "Unknown";
    break;
  }
}

const char* get_pentair_packet_type(const unsigned char* packet , int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PEN_PKT_CMD]) {
    case PEN_CMD_SPEED:
      if (packet[PEN_PKT_DEST]== PEN_DEV_MASTER)
        return "VSP SetSpeed rtn";
      else
        return "VSP SetSpeed";
    break;
     case PEN_CMD_REMOTECTL:
      if (packet[PEN_PKT_DEST]== PEN_DEV_MASTER)
        return "VSP RemoteCtl rtn";
      else
        return "VSP RemoteCtl";
    break;
     case PEN_CMD_POWER:
      if (packet[PEN_PKT_DEST]== PEN_DEV_MASTER)
        return "VSP SetPower rtn";
      else
        return "VSP SetPower";
    break;
     case PEN_CMD_STATUS:
      if (packet[PEN_PKT_DEST]== PEN_DEV_MASTER)
        return "VSP Status";
      else
        return "VSP GetStatus";
    break;
    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PEN_PKT_CMD]);
      return buf;
    break;
  }
}
const char* get_jandy_packet_type(const unsigned char* packet , int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PKT_CMD]) {
    case CMD_ACK:
      if (packet[5] == NUL)
        return "Ack";
      else
        return "Ack w/ Command";
    break;
    case CMD_STATUS:
      return "Status";
    break;
    case CMD_MSG:
      return "Message";
    break;
    case CMD_MSG_LONG:
      return "Lng Message";
    break;
    case CMD_PROBE:
      return "Probe";
    break;
    case CMD_GETID:
      return "GetID";
    break;
    case CMD_PERCENT:
      return "AR %%";
    break;
    case CMD_PPM:
      return "AR PPM";
    break;
    case CMD_PDA_0x05:
      return "PDA Unknown";
    break;
    case CMD_PDA_0x1B:
      return "PDA Init (*guess*)";
    break;
    case CMD_PDA_HIGHLIGHT:
      return "PDA Hlight";
    break;
    case CMD_PDA_CLEAR:
      return "PDA Clear";
    break;
    case CMD_PDA_SHIFTLINES:
      return "PDA Shiftlines";
    break;
    case CMD_PDA_HIGHLIGHTCHARS:
      return "PDA HlightChars";
    break;
    case CMD_IAQ_PAGE_MSG:
      if (packet[4] == 0x31)
        return "iAq setDevice";
      else
        return "iAq pMessage";
    break;
    case CMD_IAQ_PAGE_BUTTON:
       if (packet[PKT_DEST] == 0x00) // To master is iAqualink2 send command
         return "iAqualnk sendCmd";
       else
         return "iAq pButton";
    break;
    case CMD_IAQ_POLL:
      return "iAq Poll";
    break;
    case CMD_IAQ_PAGE_END:
      return "iAq PageEnd";
    break;
    case CMD_IAQ_PAGE_START:
      return "iAq PageStart";
    break;
    case CMD_IAQ_TABLE_MSG:
      return "iAq TableMsg";
    break;
    case CMD_IAQ_PAGE_CONTINUE:
      return "iAq PageContinue";
    break;
    case CMD_IAQ_STARTUP:
      return "iAq init";
    break;
    case CMD_IAQ_TITLE_MESSAGE:
      return "iAq ProductName";
    break;
    case CMD_IAQ_MSG_LONG:
      return "iAq Popup message";
    break;
    case CMD_IAQ_MAIN_STATUS:
      return "iAq Main status";
    break;
    case CMD_IAQ_1TOUCH_STATUS:
      return "iAq 1Tch status";
    break;
    case CMD_IAQ_AUX_STATUS:
      return "iAq AUX status";
    break;
    case RSSA_DEV_STATUS:
      // This is a fail reply 0x10|0x02|0x48|0x13|0x02|0x00|0x10|0x00|0x7f|0x10|0x03|
       // Rather than check all, just check 0x02 and checksum sin't I'm not sure 0x10 means faiure without 0x00 around it.
      if (packet[4] == 0x02 && packet[8] == 0x7f)
        return "RSSA Cmd Error";
      else
        return "RSSA DevStatus";
    break;
    case RSSA_DEV_READY:
      return "RSSA SendCommand";
    break;
    case CMD_EPUMP_STATUS:
      if (packet[4] == CMD_EPUMP_RPM)
        return "ePump RPM";
      else if (packet[4] == CMD_EPUMP_WATTS)
        return "ePump Watts";
      else
        return "ePump (unknown)";
    break;
    case CMD_EPUMP_RPM:
      return "ePump set RPM";
    break;
    case CMD_EPUMP_WATTS:
      return "ePump get Watts";
    break;
    case CMD_JXI_PING:
      if (packet[4] == 0x19)
        return "LXi heater on";
      else // 0x11 is normal off
        return "LXi heater ping";
    break;
    case CMD_JXI_STATUS:
      if (packet[6] == 0x10)
        return "LXi error";
      else
        return "LXi status";
    break;

    case 0x53:
      return "iAqalnk Poll";
    break;

    case CMD_IAQ_CMD_READY:
      return "iAqalnk rec ready";
    break;
    case CMD_IAQ_CTRL_READY:
      return "iAq receive ready";
    break;

    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PKT_CMD]);
      return buf;
    break;
  }
}

const char* get_packet_type(const unsigned char* packet , int length)
{
  if (getProtocolType(packet)==PENTAIR) {
    return get_pentair_packet_type(packet, length);
  }
  
  return get_jandy_packet_type(packet, length);
}


// Generate and return checksum of packet.
int generate_checksum(unsigned char* packet, int length)
{
  int i, sum, n;

  n = length - 3;
  sum = 0;
  for (i = 0; i < n; i++)
  sum += (int) packet[i];
  return(sum & 0x0ff);
}

bool check_jandy_checksum(unsigned char* packet, int length)
{
  //printf("Checking 0x%02hhx against 0x%02hhx\n",generate_checksum(packet, length), packet[length-3]);
  if (generate_checksum(packet, length) == packet[length-3])
    return true;

  // There seems to be a bug with jandy one touch protocol where on a long msg to one touch you get
  // a bad checksum but everything is actually accurate, so forcing a good return on this.
  // Example message (always one touch / status message / line 3 and always 0x0a checksum)
  // 0x10|0x02|0x43|0x04|0x03|0x20|0x20|0x20|0x20|0x35|0x3a|0x30|0x35|0x20|0x50|0x4d|0x20|0x20|0x20|0x20|0x20|0x0a|0x10|0x03|
  if (packet[3] == 0x04 && packet[4] == 0x03 && packet[length-3] == 0x0a) {
    LOG(RSSD_LOG,LOG_INFO, "Ignoring bad checksum, seems to be bug in Jandy protocol\n");
    if (getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
      static char buf[1000];
      beautifyPacket(buf,1000, packet,length,true);
      LOG(RSSD_LOG,LOG_DEBUG, "Packetin question %s\n",buf);
    }
    return true;
  }

  return false;
}

bool check_pentair_checksum(unsigned char* packet, int length)
{
  //printf("check_pentair_checksum \n");
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 8;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  //printf("Check High 0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n], packet[length-2],((sum >> 8) & 0xFF) );
  //printf("Check Low  0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n + 1], packet[length-1], (sum & 0xFF) ); 

  // Check against caculated length
  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;

  // Check against actual # length
  if (sum == (packet[n] * 256 + packet[n+1])) {
    LOG(RSSD_LOG,LOG_ERR, "Pentair checksum is accurate but length is not\n");
    return true;
  }
  
  return false;
}


void generate_pentair_checksum(unsigned char* packet, int length)
{
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 6;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  packet[n+1] = (unsigned char) (sum & 0xFF);        // Low Byte
  packet[n] = (unsigned char) ((sum >> 8) & 0xFF); // High Byte

}

protocolType getProtocolType(const unsigned char* packet) {
  if (packet[0] == DLE)
    return JANDY;
  else if (packet[0] == PP1)
    return PENTAIR;

  return P_UNKNOWN; 
}


int set_port_low_latency(int fd, const char* tty)
{

  struct serial_struct serial;

  if (ioctl (fd, TIOCGSERIAL, &serial) < 0) {
    LOG(RSSD_LOG,LOG_WARNING, "Doesn't look like your USB2RS485 device (%s) supports low latency, this might cause problems on a busy RS485 bus (%d): %s\n ", tty,errno, strerror( errno ));
    //LOG(RSSD_LOG,LOG_WARNING, "Error reading low latency mode for port %s (%d): %s\n",  tty, errno, strerror( errno ));
    return -1;
  }

  LOG(RSSD_LOG,LOG_NOTICE, "Port %s low latency mode is %s\n", tty, (serial.flags & ASYNC_LOW_LATENCY) ? "set" : "NOT set, resetting to low latency!");
  serial.flags |= ASYNC_LOW_LATENCY;

  if (ioctl (fd, TIOCSSERIAL, &serial) < 0) {
		LOG(RSSD_LOG,LOG_ERR, "Unable to set port %s to low latency mode (%d): %s\n", tty, errno, strerror( errno ));
    return -1;
	}

  return 0;
}

#include <sys/file.h>

int lock_port(int fd, const char* tty)
{
  if (ioctl (fd, TIOCEXCL) < 0) {
    LOG(RSSD_LOG,LOG_ERR, "Can't put (%s) into exclusive mode (%d): %s\n", tty,errno, strerror( errno ));
    return -1;
  }
/*
  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    LOG(RSSD_LOG,LOG_ERR, "Can't lock (%s) (%d): %s\n", tty,errno, strerror( errno ));
    return -1;
  }
*/
  return 0;
}

int unlock_port(int fd)
{
  if (ioctl(fd, TIOCNXCL) < 0) {
    LOG(RSSD_LOG,LOG_ERR, "Failed to remove into exclusive mode (%d): %s\n", errno, strerror( errno ));
  }

  /*
  if (flock(fd, LOCK_UN) < 0) {
    //if (!isAqualinkDStopping()) {
      LOG(RSSD_LOG,LOG_ERR, "Can't unlock serial port (%d): %s\n",errno, strerror( errno ));
    //}
    return -1;
  }
*/
  return 0;
}

int is_valid_port(int fd) {
  return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}


void print_file_flags(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return;
    }
    if (flags & O_RDONLY) {
        printf("  - O_RDONLY (Read-only access)\n");
    }
    if (flags & O_WRONLY) {
        printf("  - O_WRONLY (Write-only access)\n");
    }
    if (flags & O_RDWR) {
        printf("  - O_RDWR (Read/write access)\n");
    }
    if (flags & O_ACCMODE) {
        printf("  - O_ACCMODE (mask for above modes)\n");
    }
    if (flags & O_NONBLOCK) {
        printf("  - O_NONBLOCK (Non-blocking I/O)\n");
    }
    if (flags & O_APPEND) {
        printf("  - O_APPEND (Append mode)\n");
    }
    /*
    if (flags & O_SHLOCK) {
        printf("  - O_SHLOCK (open with shared file lock)\n");
    }
     if (flags & O_EXLOCK) {
        printf("  - O_EXLOCK (open with exclusive file lock)\n");
    }
        */
    if (flags & O_ASYNC) {
        printf("  - O_ASYNC (Asynchronous I/O signal)\n");
    }
    if (flags & O_NOFOLLOW) {
        printf("  - O_NOFOLLOW (don't follow symlinks)\n");
    }
    if (flags & O_DSYNC) {
        printf("  - O_DSYNC (Synchronous I/O data integrity)\n");
    }
    if (flags & O_SYNC) {
        printf("  - O_SYNC (Synchronous I/O file integrity)\n");
    }
    if (flags & O_NOCTTY) {
        printf("  - O_NOCTTY (don't assign controlling terminal)\n");
    }
}

// https://www.cmrr.umn.edu/~strupp/serial.html#2_5_2
// http://unixwiz.net/techtips/termios-vmin-vtime.html

int init_serial_port(const char* port)
{
  struct termios tty;

  // Have to open with O_NONBLOCK so we don't wait for the Data Carrier Detect (DCD) signal to go high
  _RS485_fds = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC); 

  if (_RS485_fds < 0)  {
    LOG(RSSD_LOG,LOG_ERR, "Unable to open port: %s: %s\n", port, strerror(errno));
    return -1;
  }

  LOG(RSSD_LOG,LOG_DEBUG, "Openeded serial port %s\n",port);
  //print_file_flags(fd);

  if ( lock_port(_RS485_fds, port) < 0) {
    LOG(RSSD_LOG,LOG_ERR, "Unable to lock port: %s: %s\n", port, strerror(errno));
    //return -1;
  }

  if (_aqconfig_.ftdi_low_latency)
    set_port_low_latency(_RS485_fds, port);
  
  if (tcgetattr(_RS485_fds, &tty) != 0) {
    LOG(RSSD_LOG,LOG_ERR, "Unable to get port attributes: %s: %s\n", port,strerror(errno));
    return -1;
  }

  // Set up Modbus protocol in raw mode (no canonical processing)
  // This function automatically unsets ICRNL and other processing flags. (stops conversion of 0x0d to 0x0a rc to lf)
  cfmakeraw(&tty); // Going to be more precise using below

  // Set baud rates to 9600
  cfsetispeed(&tty, B9600);
  cfsetospeed(&tty, B9600);

  // 8 data bits (CS8), no parity (PARENB cleared), 1 stop bit (CSTOPB cleared)
  // Note: CSIZE is a mask, so we must first clear it before setting CS8.
  tty.c_cflag &= ~PARENB; // No parity
  tty.c_cflag &= ~CSTOPB; // 1 stop bit
  tty.c_cflag &= ~CSIZE;  // Clear all data bit size flags.
  tty.c_cflag |= CS8;     // 8 data bits.

  // Disable hardware (RTS/CTS) and software (XON/XOFF) flow control
  tty.c_cflag &= ~CRTSCTS; // Disable hardware flow control
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control

  // Set other control options for "raw" mode
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Non-canonical input, disable echo
  tty.c_oflag &= ~OPOST; // Raw output

  // Enable receiver and ignore modem control lines
  tty.c_cflag |= (CREAD | CLOCAL);

  // Set timeout for read operations
  // VMIN = 0, VTIME > 0 for a read timeout
  // In this case, 1 second (10 * 0.1s) timeout.
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 10;

  // Below resets the open with O_NONBLOCK
  //fcntl(fd, F_SETFL, 0);
  
  // Write the modified settings
  if (tcsetattr(_RS485_fds, TCSANOW, &tty) != 0) {
    LOG(RSSD_LOG,LOG_ERR,"Error %i from tcsetattr: %s\n", errno, strerror(errno));
    return -1;
  }

  // Clear out buffer
  if (tcflush(_RS485_fds, TCIFLUSH) == -1) {
    LOG(RSSD_LOG,LOG_ERR,"Error %i from tcflush: %s\n", errno, strerror(errno));
  }

  return _RS485_fds;
}

/* close tty port */
void _close_serial_port(int fd)
{
  if ( fcntl(fd, F_GETFD, 0) == -1 || errno == EBADF ) {
    // Looks like bad fd or already closed. return with no error since we can get called twice
    return;
  }

  unlock_port(fd);
  close(fd);
  LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Closed serial port\n");
}

// Can pass NULL for serialport
void close_serial_port(int port)
{
  if (port >= 0) {
    _close_serial_port(port);
  } else {
    if (_RS485_fds < 0) {
      LOG(RSSD_LOG,LOG_ERR, "Didn't find valid serial port file descriptor to close\n");
      return;
    }
    LOG(RSSD_LOG,LOG_NOTICE, "Closing serial port\n");
    _close_serial_port(_RS485_fds);
  }
}


// Send an ack packet to the Aqualink RS8 master device.
// fd: the file descriptor of the serial port connected to the device
// command: the command byte to send to the master device, NUL if no command
// 
// NUL = '\x00'
// DLE = '\x10'
// STX = '\x02'
// ETX = '\x03'
// 
// masterAddr = '\x00'          # address of Aqualink controller
// 
//msg = DLE+STX+dest+cmd+args
//msg = msg+self.checksum(msg)+DLE+ETX
//      DLE+STX+DEST+CMD+ARGS+CHECKSUM+DLE+ETX

/*
void print_hex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
  printf("\n");
}
*/



void send_pentair_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;

  packet[0] = NUL;
  packet[1] = PP1;
  packet[2] = PP2;
  packet[3] = PP3;
  packet[4] = PP4;

  //packet[i++] = 0x00; // from
  //packet[i++] = // to
  for (i=5; i-5 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-4],i);
    if (i==6) {
      // Replace source
      //packet[i] = 0x00;
      // Don't replace source
      packet[i] = packet_buffer[i-5];
    } else if (i==9) {
      // Replace length
      //packet[i] = 0xFF;
      packet[i] = (unsigned char)size-5;
    } else {
      packet[i] = packet_buffer[i-5];
    }

    //packet[i] = packet_buffer[i-4];    
  }

  packet[++i] = NUL;  // Checksum
  packet[++i] = NUL;  // Checksum
  generate_pentair_checksum(&packet[1], i);
  //packet[++i] = NUL;


  //logPacket(packet, i);
  send_packet(fd,packet,++i);
}

#ifndef SEND_CMD_WITH_TRAILING_NUL

void send_jandy_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;
  
  packet[0] = NUL;
  packet[1] = DLE;
  packet[2] = STX;

  for (i=3; i-3 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-2],i);
    packet[i] = packet_buffer[i-3];
  }

  packet[++i] = DLE;
  packet[++i] = ETX;

  packet[i-2] = generate_checksum(packet, i+1);

  send_packet(fd,packet,++i);
}
#else
void send_jandy_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;
  
  packet[0] = DLE;
  packet[1] = STX;

  for (i=2; i-2 < size; i++) {
    packet[i] = packet_buffer[i-2];
  }

  packet[++i] = DLE;
  packet[++i] = ETX;
  packet[++i] = NUL;

  packet[i-3] = generate_checksum(packet, i);

  send_packet(fd,packet,++i);
}
#endif
/*
 unsigned char tp[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
 send_command(0, tp, 19);
 Should produce
{0xFF, 0x00, 0xFF, 0xA5, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x02, 0x9E};
 <-------  headder ----> <-- type to from type-> <len> <------------------------------ data ----------------------------------------> <checksum>
*/
//unsigned char packet_buffer[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
//unsigned char packet_buffer[] = {PCOL_JANDY, 0x07, 0x0F, 0x00, 0x00};
void send_command(int fd, unsigned char *packet_buffer, int size)
{
  //unsigned char packet[AQ_MAXPKTLEN];
  //int i=0;
  
  if (packet_buffer[0] == PCOL_JANDY) {
    //LOG(RSSD_LOG,LOG_ERR, "Only Jandy protocol supported at present!\n");
    send_jandy_command(fd, &packet_buffer[1], size-1);
    return;
  }
  if (packet_buffer[0] == PCOL_PENTAIR) {
    //LOG(RSSD_LOG,LOG_ERR, "Only Jandy protocol supported at present!\n");
    send_pentair_command(fd, &packet_buffer[1], size-1);
    return;
  }
}

/* Clean out any NULL in serial line.
 * Return 0x00 if nothing or only nuls read, or return byte if something else was read
 * leave fullclean as option as may want to use this to cleanout end of message on get_packet function
 */
 unsigned char cleanOutSerial(int fd, bool fullClean)
 {
   unsigned char byte = 0x00;
   while ( (read(fd, &byte, 1) == 1) && (byte == 0x00 || fullClean) ) {
     //printf("*** Peek Read 0x%02hhx ***\n",byte);
   }
   return byte;
 }  


/*
NEWPACKETADDRESSSPACE 
is test to copy packet to unused address space before send, just incase tc_drain doesn't work correctly 
and we change buffer after function call.  (shouldn;t even happen / be needed buyt good for testing)
*/
#ifdef NEWPACKETADDRESSSPACE
void send_packet(int fd, unsigned char *packet_buffer, int length)
{
  static unsigned char packet[AQ_MAXPKTLEN];
  memcpy(packet, packet_buffer, length);
#else
void send_packet(int fd, unsigned char *packet, int length)
{
#endif

  struct timespec elapsed_time;
  struct timespec now;

  if (_aqconfig_.frame_delay > 0) {
    struct timespec min_frame_wait_time;
    struct timespec frame_wait_time;
    struct timespec remainder_time;
    //struct timespec diff;

    min_frame_wait_time.tv_sec = 0;
    min_frame_wait_time.tv_nsec = _aqconfig_.frame_delay * 1000000;

    do {
      clock_gettime(CLOCK_REALTIME, &now);
      timespec_subtract(&elapsed_time, &now, &_last_serial_read_time);
      if (timespec_subtract(&frame_wait_time, &min_frame_wait_time, &elapsed_time)) {
        break;
      }
    } while (nanosleep(&frame_wait_time, &remainder_time) != 0);
  }

  clock_gettime(CLOCK_REALTIME, &now);

  if (true) {
    //int nwrite = write(fd, packet, length);
    //LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes of %d\n",nwrite,length);
    int nwrite = write(fd, packet, length);
    //if (nwrite < 0)
    if (nwrite != length)
        LOG(RSSD_LOG, LOG_ERR, "write to serial port failed\n");
  } else {
    int nwrite, i;
    for (i = 0; i < length; i += nwrite) {
      nwrite = write(fd, packet + i, length - i);
      if (nwrite < 0)
        LOG(RSSD_LOG, LOG_ERR, "write to serial port failed\n");
      //else
      //  LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes of %d\n",nwrite,length-1);
    }
  } // _blockine_mode

  /*
  #ifdef AQ_DEBUG
    // Need to take this out for release
    if ( getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
      debuglogPacket(&packet[1], length-2);
    }
  #endif
  */

  // MAYBE Change this back to debug serial
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",length-2);
  //LOG(RSSD_LOG,LOG_DEBUG, "Serial write %d bytes, type 0x%02hhx cmd 0x%02hhx\n",length-2,packet[5],packet[6]);
  if (_aqconfig_.log_protocol_packets || getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL)
    logPacketWrite(&packet[1], length-1);
/*
  if (getLogLevel(PDA_LOG) == LOG_DEBUG) {
    char buff[1024];
    beautifyPacket(buff, &packet[1], length, false);
    LOG(PDA_LOG,LOG_DEBUG, "%s", buff);
  }
*/
/*
  if ( getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL || ) {
    // Packet is padded with 0x00, so discard for logging
    LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial write %d bytes\n",length-2);
    logPacket(&packet[1], length-2);
  }*/ /*else if (getLogLevel(RSSD_LOG) >= LOG_DEBUG) {
    static char buf[1000];
    beautifyPacket(buf,&packet[1],length-2);
    LOG(RSSD_LOG,LOG_DEBUG, "Serial write %s\n",buf);
  }*/

  tcdrain(fd); // Make sure buffer has been sent.
  //if (_aqconfig_.frame_delay > 0) {
#ifndef SERIAL_LOGGER
  if (_aqconfig_.frame_delay > 0) {
    timespec_subtract(&elapsed_time, &now, &_last_serial_read_time);
    LOG(RSTM_LOG, LOG_DEBUG, "Time from recv to send is %.3f sec\n",
                            roundf3(timespec2float(&elapsed_time)));
  }
#endif
  //}

}

#ifndef SEND_CMD_WITH_TRAILING_NUL
void _send_ack(int fd, unsigned char ack_type, unsigned char command)
{
  //const int length = 11;
  int length = 10;
  // Default null ack with checksum generated, don't mess with it, just over right                    
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL }; 
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX }; 

  // To overcome Pentair VSP bug in Jandy control panel, we need to NOT send trailing NUL on
  // a normal ACK, but sent the trailing NUL on in ack with command.
  // Always send trailing NUL causes VSP to loose connection
  // Never sending trailing NUL causes come commands to be missed.

  // Update the packet and checksum if command argument is not NUL.
  if(command != NUL || ack_type != NUL) {
    //ackPacket[5] = 0x00 normal, 0x03 some pause, 0x01 some pause ending  (0x01 = Screen Busy (also return from logn message))
    ackPacket[5] = ack_type;
    ackPacket[6] = command;
    ackPacket[7] = generate_checksum(ackPacket, length);
    if (command == DLE) {  // We shuld probably also check the ack type as well, just for future proofing.
      // 0x10(DLE) that's not part of the headder or footer needs to be escaped AFTER with NUL, so shif everyting uo one
      ackPacket[8] = ackPacket[7]; // move the caculated checksum
      ackPacket[7] = NUL; // escape the DLE
      ackPacket[9] = DLE; // add new end sequence 
      ackPacket[10] = ETX; // add new end sequence
      length = 11;
    }
  }
  send_packet(fd, ackPacket, length);
}
#else
void _send_ack(int fd, unsigned char ack_type, unsigned char command)
{
  //const int length = 11;
  int length = 9;
  // Default null ack with checksum generated, don't mess with it, just over right                    
  unsigned char ackPacket[] = { DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL }; 

  if(command != NUL || ack_type != NUL) {
    //ackPacket[5] = 0x00 normal, 0x03 some pause, 0x01 some pause ending  (0x01 = Screen Busy (also return from logn message))
    ackPacket[4] = ack_type;
    ackPacket[5] = command;
    ackPacket[6] = generate_checksum(ackPacket, length);
    if (command == DLE) {  // We shuld probably also check the ack type as well, just for future proofing.
      // 0x10(DLE) that's not part of the headder or footer needs to be escaped AFTER with NUL, so shif everyting uo one
      ackPacket[7] = ackPacket[7]; // move the caculated checksum
      ackPacket[6] = NUL; // escape the DLE
      ackPacket[8] = DLE; // add new end sequence 
      ackPacket[9] = ETX; // add new end sequence
      length = 10;
    }
  }
  send_packet(fd, ackPacket, length);
}
#endif // SEND_CMD_WITH_TRAILING_NUL

void send_ack(int fd, unsigned char command)
{
   _send_ack(fd, ACK_NORMAL, command);
}

// ack_typ should only be ACK_PDA, ACK_NORMAL, ACK_SCREEN_BUSY, ACK_SCREEN_BUSY_DISPLAY
void send_extended_ack(int fd, unsigned char ack_type, unsigned char command)
{
  _send_ack(fd, ack_type, command);
}
/*
int _get_packet(int fd, unsigned char* packet, bool rawlog);

int get_packet(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, false);
}
int get_packet_lograw(int fd, unsigned char* packet)
{
  return _get_packet(fd, packet, true);
}
int _get_packet(int fd, unsigned char* packet, bool rawlog)
*/

#ifdef DUMMY_READER

int fix_packet(unsigned char *packet_buffer, int packet_length, bool getCached) {
  
  static unsigned char saved_buffer[AQ_MAXPKTLEN+1];
  static int saved_buffer_length = 0;

  if (getCached) {
    if (saved_buffer_length > 0) {
      memcpy(packet_buffer, saved_buffer, saved_buffer_length);
      LOG(RSSD_LOG,LOG_DEBUG, "2nd part of fixed frame\n");
      logPacket(RSSD_LOG, LOG_DEBUG, packet_buffer, saved_buffer_length, true);
      debuglogPacket(RSSD_LOG, packet_buffer, saved_buffer_length, true, true);
    } else {
      printf("NO 2nd PACKET %d\n",saved_buffer_length );
    }
    return saved_buffer_length;
  }

  char pbuf[256];
  int rtn = 0;

  memset(saved_buffer, 0, sizeof(saved_buffer));
  saved_buffer_length = 0;

  //LOG(RSSD_LOG,LOG_DEBUG, "Trying to fix bad packet\n");
  LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, Trying to fix bad packet\n");
  // Check end is valid

  if ( packet_buffer[packet_length-2] == DLE  && packet_buffer[packet_length-1] == ETX ) {
    //LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: good end\n");
  } else {
    LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: bad end\n");
    return 0;
  }

  // Specific fix for 2 frames where the first didn't end (but still holds the checksum)
  // HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x02|0x84|0x00|0x96|0x10|0x03|       <- no end 0x10|0x03, start 0x10|0x02 in middle
  // HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x10|0x02|0x33|0x30|0x75|0x10|0x03|  <- no 0x03 for end, start 0x10|0x02 in middle

  // Run over the packet, see if their is a start in the middle.
  // Ignore the first two bytes, assuming they are 0x10, 0x02
  

  for (int i=2; i <= packet_length; i++ ) {
    if (packet_buffer[i] == DLE && packet_buffer[i+1] == STX) {
      LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: found start in middle of frame\n");
      int p1_length = i;
      int p2_length = packet_length-i;
      unsigned char *p2_start =  &packet_buffer[i];
      bool validstart=FALSE;
      bool validend=FALSE;
      // Something we catch the end DLE as well as start DLE
      while (packet_buffer[p1_length] == DLE) {
        p1_length--;
      }

      beautifyPacket(pbuf, 256, packet_buffer, packet_length, TRUE);
      LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: SPLITTING start to: %s\n",pbuf);
      LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: Check 0x%02hhx (%d)\n",packet_buffer[p1_length], p1_length);
      
        // see if we have a valid end
      if (check_jandy_checksum(p2_start, p2_length) == true){
          //LOG(RSSD_LOG,LOG_DEBUG, "Valid packet found at end\n");
        beautifyPacket(pbuf, 256, p2_start, p2_length, TRUE);
        LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: FIXED end to:   %s\n",pbuf);
        validend=TRUE;
      }

      if (check_jandy_checksum(packet_buffer, p1_length+3) == true){
        //LOG(RSSD_LOG,LOG_DEBUG, "Valid packet found at start\n");
        beautifyPacket(pbuf, 256, packet_buffer, p1_length, TRUE);
        LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: FIXED start to: %s\n",pbuf);
        validstart=TRUE;
      }
      
      if (validstart && validend) {
        // Save the 2nd frame
        memcpy(saved_buffer, p2_start, p2_length);
        saved_buffer_length = p2_length;
        // correct the 1st frame
        packet_buffer[p1_length+1] = DLE;
        packet_buffer[p1_length+2] = ETX;
        rtn = p1_length+3;
      } else if (validend) {
        // Return the valid part of the packet.
        memcpy(packet_buffer, p2_start, p2_length);
        rtn = p2_length;
      } else if (validstart) {
        // correct the 1st frame
        packet_buffer[p1_length+1] = DLE;
        packet_buffer[p1_length+2] = ETX;
        rtn = p1_length+3;
      }
      // Try the start.
      //memcpy(new_buffer, packet_buffer, i);
      
      break;
    }
  }

  beautifyPacket(pbuf, 256, packet_buffer, rtn, TRUE);
  LOG(RSSD_LOG,LOG_DEBUG, "Packet Fix: RETURNING:   %s\n",pbuf);

  return rtn;
}

#endif



/*
LXi status | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x00|0x1f|0x10|0x03|
             HEX: 0x10|0x02|0x00|0x0a|0x00|0x00|0x00|0x1f|0x10|0x03| // New read seems to use 0x0a and not 0x0d
*/


int get_packet(int fd, unsigned char* packet)
{
  unsigned char byte = 0x00;
  int bytesRead;
  int index = 0;
  bool endOfPacket = false;
  //bool packetStarted = FALSE;
  bool lastByteDLE = false;
  int retry = 0;
  bool jandyPacketStarted = false;
  bool pentairPacketStarted = false;
  //bool lastByteDLE = false;
  int PentairPreCnt = 0;
  int PentairDataCnt = -1;
  struct timespec packet_elapsed;
  struct timespec packet_end_time;

  int wait_val;
  struct timeval read_tv;
  read_tv.tv_sec = SERIAL_READ_TIMEOUT_SEC;  // 1-second timeout
  read_tv.tv_usec = 0;

  memset(packet, 0, AQ_MAXPKTLEN);

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

  while (!endOfPacket) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    // Wait for up to read_tv.tv_sec second for data to become available
    wait_val = select(fd + 1, &readfds, NULL, NULL, &read_tv);
    if (wait_val == -1) {
      return AQSERR_READ;
    } else if (wait_val == 0) {
      //return AQSERR_TIMEOUT;
      return 0; // Should probably change to above
    }

    bytesRead = read(fd, &byte, 1);

    if (bytesRead <= 0 && errno == EAGAIN ) { // We also get ENOTTY on some non FTDI adapters
      if (jandyPacketStarted == false && pentairPacketStarted == false && lastByteDLE == false) {
        return 0;
      } else if (++retry > 10 ) {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read timeout\n");
        if (index > 0) { logPacketError(packet, index); }
        return AQSERR_READ;
      } else {
        continue;
      }
    } else if(bytesRead <= 0) {
      if (! isAqualinkDStopping() ) {
        return AQSERR_READ;
      } else {
        return 0;
      }
    } else if (bytesRead == 1) {
      retry = 0;
      if (_aqconfig_.log_raw_bytes)
        logPacketByte(&byte);

      if (lastByteDLE == true && byte == NUL)
      {
        // Check for DLE | NULL (that's escape DLE so delete the NULL)
        //printf("IGNORE THIS PACKET\n");
        lastByteDLE = false;
      }
      else if (lastByteDLE == true)
      {
        if (index == 0)
          index++;

        packet[index] = byte;
        index++;
        if (byte == STX && jandyPacketStarted == false)
        {
          jandyPacketStarted = true;
          pentairPacketStarted = false;
        }
        else if (byte == ETX && jandyPacketStarted == true)
        {
          endOfPacket = true;
        }
      }
      else if (jandyPacketStarted || pentairPacketStarted)
      {
        packet[index] = byte;
        index++;
        if (pentairPacketStarted == true && index == 9)
        {
          //printf("Read 0x%02hhx %d pentair\n", byte, byte);
          PentairDataCnt = byte;
        }
        if (PentairDataCnt >= 0 && index - 11 >= PentairDataCnt && pentairPacketStarted == true)
        {
          endOfPacket = true;
          PentairPreCnt = -1;
        }
      }
      else if (byte == DLE && jandyPacketStarted == false)
      {
        packet[index] = byte;
      }

      // // reset index incase we have EOP before start
      if (jandyPacketStarted == false && pentairPacketStarted == false)
      {
        index = 0;
      }

      if (byte == DLE && pentairPacketStarted == false)
      {
        lastByteDLE = true;
        PentairPreCnt = -1;
      }
      else
      {
        lastByteDLE = false;
        if (byte == PP1 && PentairPreCnt == 0)
          PentairPreCnt = 1;
        else if (byte == PP2 && PentairPreCnt == 1)
          PentairPreCnt = 2;
        else if (byte == PP3 && PentairPreCnt == 2)
          PentairPreCnt = 3;
        else if (byte == PP4 && PentairPreCnt == 3)
        {
          pentairPacketStarted = true;
          jandyPacketStarted = false;
          PentairDataCnt = -1;
          packet[0] = PP1;
          packet[1] = PP2;
          packet[2] = PP3;
          packet[3] = byte;
          index = 4;
        }
        else if (byte != PP1) // Don't reset counter if multiple PP1's
          PentairPreCnt = 0;
      }
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet too large for buffer, stopped reading\n");
      logPacketError(packet, index);
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return AQSERR_2LARGE;
      break;
    }
  }

  // Report any unusual size packets.
  if (index >= AQ_MAXPKTLEN_WARNING) {
    // Aqualink2 packets 0x72 and 0x71 can be very large, so supress if it one of those.
    if (packet[PKT_CMD] != CMD_IAQ_AUX_STATUS && packet[PKT_CMD] != CMD_IAQ_1TOUCH_STATUS) {
      LOG(RSSD_LOG,LOG_WARNING, "Serial packet seems too large at length %d\n", index);
      logPacketError(packet, index);
    }
  }

  //LOG(RSSD_LOG,LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));
  if (jandyPacketStarted) {
    if (check_jandy_checksum(packet, index) != true) {
#ifdef DUMMY_READER
      int size = fix_packet(packet, index, false);
      if (size > 0) {
        haveFixedPacket = true;
        index = size;
      } else
#endif
      {
        LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
        logPacketError(packet, index);
        return AQSERR_CHKSUM;
      }
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      LOG(RSSD_LOG,LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      logPacketError(packet, index);
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return AQSERR_CHKSUM;
    }
  }

 
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    LOG(RSSD_LOG,LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ 
  if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    LOG(RSSD_LOG,LOG_WARNING, "Serial read too small\n");
    logPacketError(packet, index);
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return AQSERR_2SMALL;
  }


  if (_aqconfig_.frame_delay > 0 || getLogLevel(RSTM_LOG) >= LOG_DEBUG) {
    clock_gettime(CLOCK_REALTIME, &packet_end_time);
    if (getLogLevel(RSTM_LOG) >= LOG_DEBUG) {
      timespec_subtract(&packet_elapsed, &packet_end_time, &_last_serial_read_time);
      /*
      LOG(RSTM_LOG, LOG_NOTICE, "Start sec=%ld nsec=%ld\n",_last_serial_read_time.tv_sec,_last_serial_read_time.tv_nsec);
      LOG(RSTM_LOG, LOG_NOTICE, "End sec=%ld nsec=%ld\n",packet_end_time.tv_sec,packet_end_time.tv_nsec);
      LOG(RSTM_LOG, LOG_NOTICE, "Elapsed sec=%ld nsec=%ld Time between packets (%.3f sec)\n",packet_elapsed.tv_sec,packet_elapsed.tv_nsec, roundf3(timespec2float(&packet_elapsed)) );
      */
      LOG(RSTM_LOG, LOG_DEBUG, "Time between packets (%.3f sec)\n", roundf3(timespec2float(&packet_elapsed)) );

    }
    //if (_aqconfig_.frame_delay > 0) {
    memcpy(&_last_serial_read_time, &packet_end_time, sizeof(struct timespec));
    //}
  }

  //clock_gettime(CLOCK_REALTIME, &_last_serial_read_time);
  //}
  //LOG(RSSD_LOG,LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  if (_aqconfig_.log_protocol_packets || getLogLevel(RSSD_LOG) >= LOG_DEBUG_SERIAL)
    logPacketRead(packet, index);
  // Return the packet length.
  return index;
}

































