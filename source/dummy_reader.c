#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <fcntl.h>
#include <time.h>

// #include "serial_logger.h"
#include "aq_serial.h"
#include "utils.h"
#include "packetLogger.h"
#include "rs_msg_utils.h"

#define CONFIG_C // Make us look like config.c when we load config.h so we get globals.
#include "config.h"

bool _keepRunning = true;
int _rs_fd;

void intHandler(int dummy)
{
  _keepRunning = false;
  LOG(SLOG_LOG, LOG_NOTICE, "Stopping!\n");
}

bool isAqualinkDStopping() {
  return !_keepRunning;
}

int get_bytes(FILE *fd, unsigned char* buffer)
{
  int packet_length = 0;
  char line[4000];
  char hex[6];
  int i;
  bool foundHex=false;
  /*
    const char *hex_string = "0xFF";
    unsigned char value;

    // Use strtoul to convert the hex string to an unsigned long
    // The third argument, 16, specifies the base (hexadecimal)
    unsigned long temp_value = strtoul(hex_string, NULL, 16);

    // Cast the unsigned long to unsigned char
    value = (unsigned char)temp_value;
  */

  if ( fgets ( line, sizeof line, fd ) != NULL ) /* read a line */
  {
    packet_length=0;
    for (i=0; i < strlen(line); i++)
    {
      if (line[i] == 'H' && line[i+1] == 'E' && line[i+2] == 'X' && line[i+3] == ':') {
        foundHex=true;
        i=i+4;
      }
      if (line[i] == '0' && line[i+1] == 'x' && foundHex) {
        break;
      }
      if (i<=1 && line[i] == '0' && line[i+1] == 'x') {
        //printf(stdout,"Line starting with hex\n");
        break;
      }
    }

    if (i == strlen(line)) {
      //printf( "PLAYBACK No binary\n");
      return 0;
    } else {
      //printf(" Transposed from index %d=",i);
      //printf( "%s",line);
      LOG(SLOG_LOG, LOG_DEBUG, "Read bytes %s", line);
    }

    for (i=i; i < strlen(line); i=i+5)
    {
      strncpy(hex, &line[i], 4);
      hex[5] = '\0';
      buffer[packet_length] = (int)strtol(hex, NULL, 16);
      packet_length++;
    }
    packet_length--;

    //printf("End Char = 0x%02hhx\n",buffer[packet_length-1]);
    buffer[packet_length] = '\0';

    return packet_length;
  }
  return -1;
}

bool createBinaryFile(char *dest, char *source) {
  int size = 0;
  unsigned char buffer[4000];

  FILE *sfp = fopen ( source, "r" );
  if ( sfp == NULL )
  {
    perror ( source ); /* why didn't the file open? */
    return FALSE;
  }
  
  FILE *dfp = fopen ( dest, "wb" );
  if ( dfp == NULL )
  {
    perror ( dest ); /* why didn't the file open? */
    fclose(sfp);
    return FALSE;
  }

  while (size != -1) {
     size = get_bytes(sfp, buffer);

     if (size > 0) {
        //printf("GOT %d bytes\n",size);
       //fputs(buffer, dfp);
       fwrite(&buffer, sizeof(unsigned char), size, dfp);

       //char pbuf[256];
       //beautifyPacket(pbuf, 256, buffer, size, TRUE);
       //printf("%s\n",pbuf);
     }
     //printf("-----------------\n");
     //if (buffer[3] == 0x72) {
     // print72pck(buffer, size);
      //process_iAqualinkStatusPacket(buffer, size);
     //}
  }

  fclose(sfp);
  fclose(dfp);

  return TRUE;
}


int main(int argc, char *argv[])
{
  int logLevel = LOG_DEBUG;
  int fp;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN+1];

  if (argc < 2 || access(argv[1], F_OK) == -1)
  {
    fprintf(stderr, "ERROR, first param must be valid filename\n");
    return 1;
  }
  setLoggingPrms(logLevel, false, NULL);

  LOG(SLOG_LOG, LOG_INFO, "Start reading %s\n", basename(argv[1]));

  createBinaryFile("/tmp/tmp.tmp", argv[1]);

  
  if ((fp = open("/tmp/tmp.tmp", O_RDONLY)) == -1)
  {
    fprintf(stderr, "Cannot open %s\n",argv[1]);
    return 1;
  }

  //packet_length = get_packet(fp, packet_buffer);

  while ((packet_length = get_packet(fp, packet_buffer)) != 0) {
    LOG(SLOG_LOG, LOG_INFO, "Read %d bytes\n", packet_length);
  }
/*
  if (packet_length == 0)
  {
    LOG(SLOG_LOG, LOG_ERR, "Error Read, %d\n", packet_length);
    close(fp);
    return 1;
  } else {
    LOG(SLOG_LOG, LOG_INFO, "Read %d bytes\n", packet_length);
  }
*/  

  LOG(SLOG_LOG, LOG_INFO, "Stopping!\n");
  close(fp);

  return 0;
}