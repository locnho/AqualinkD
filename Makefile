#
# Options
#
# make          // standard build aqualinkd and serial_logger
# make debug    // Compule standard aqualinkd binary just with debugging
# make aqdebug  // Compile with extra aqualink debug information like timings
# make slog     // Serial logger
# make <other>  // not documenting
#

# Valid flags for AQ_FLAGS
#AQ_RS16 = true
AQ_PDA  = true
AQ_MANAGER = true

#AQ_CONTAINER = false // this is for compiling for containers

# define the C compiler(s) to use
CC = gcc
CC_ARM64 = aarch64-linux-gnu-gcc
CC_ARMHF = arm-linux-gnueabihf-gcc
CC_AMD64 = x86_64-linux-gnu-gcc

#LIBS := -lpthread -lm
# from documentation -lrt would be needed for glibc 2.17 & prior (debug clock realtime messages), but seems to be needed for armhf 2.24
LIBS := -lpthread -lm -lrt

# Standard compile flags
GCCFLAGS = -Wall -O3
#GCCFLAGS = -Wall -O3 -Wunused-macros
#GCCFLAGS = -Wall -O3 -Wextra
#GCCFLAGS = -Wall -O3 -ffunction-sections -fdata-sections

# Standard debug flags
DGCCFLAGS = -Wall -O0 -g

# Aqualink Debug flags
DBGFLAGS = -g -O0 -Wall -D AQ_DEBUG -D AQ_TM_DEBUG

# Mongoose 7.19 flags
#MGFLAGS = -D MG_TLS=2 #(2=MG_TLS_OPENSSL. 3=MG_TLS_BUILTIN) --or--  -DMG_TLS=MG_TLS_BUILTIN
MGFLAGS = -D MG_TLS=3 -D MG_ENABLE_SSI=0

# Detect OS and set some specifics
ifeq ($(OS),Windows_NT)
   # Windows Make.
   MKDIR = mkdir
   FixPath = $(subst /,\,$1)
else
   UNAME_S := $(shell uname -s)
   # Linux
   ifeq ($(UNAME_S),Linux)
	  MKDIR = mkdir -p
    FixPath = $1
	  # Get some system information
    #  PI_OS_VERSION = $(shell cat /etc/os-release | grep VERSION= | cut -d\" -f2)
    #  $(info OS: $(PI_OS_VERSION) )
    #  GLIBC_VERSION = $(shell ldd --version | grep ldd)
    #  $(info GLIBC build with: $(GLIBC_VERSION) )
    #  $(info GLIBC Prefered  : 2.24-11+deb9u1 2.24 )
   endif
   # OSX
   ifeq ($(UNAME_S),Darwin)
      MKDIR = mkdir -p
      FixPath = $1
   endif
endif


# Main source files
SRCS = aqualinkd.c utils.c config.c aq_serial.c aq_panel.c aq_programmer.c allbutton.c allbutton_aq_programmer.c net_services.c net_interface.c json_messages.c rs_msg_utils.c\
       onetouch.c onetouch_aq_programmer.c iaqtouch.c iaqtouch_aq_programmer.c iaqualink.c\
       devices_jandy.c packetLogger.c devices_pentair.c color_lights.c serialadapter.c aq_timer.c aq_scheduler.c web_config.c\
       serial_logger.c mongoose.c mqtt_discovery.c simulator.c sensors.c aq_systemutils.c timespec_subtract.c auto_configure.c chem_feeder_mqtt.c


AQ_FLAGS =
# Add source and flags depending on protocols to support.
ifeq ($(AQ_PDA), true)
  SRCS := $(SRCS) pda.c pda_menu.c pda_aq_programmer.c
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_PDA
endif

ifeq ($(AQ_MANAGER), true)
  AQ_FLAGS := $(AQ_FLAGS) -D AQ_MANAGER
  LIBS := $(LIBS) -lsystemd
  # aq_manager requires threads, so make sure that's turned on.
  ifeq ($(AQ_NO_THREAD_NETSERVICE), true)
    # Show error
    $(warning AQ_MANAGER requires threads, ignoring AQ_NO_THREAD_NETSERVICE)
  endif
else
  # No need for serial_logger without aq_manager
  SRCS := $(filter-out serial_logger.c, $(SRCS))
  # no threadded net service only valid without aq manager.
  ifeq ($(AQ_NO_THREAD_NETSERVICE), true)
    AQ_FLAGS := $(AQ_FLAGS) -D AQ_NO_THREAD_NETSERVICE
  endif
endif


# Put all flags together.
CFLAGS = $(GCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DFLAGS = $(DGCCFLAGS) $(AQ_FLAGS) $(MGFLAGS)
DBG_CFLAGS = $(DBGFLAGS) $(AQ_FLAGS) $(MGFLAGS)

# Other sources.
DBG_SRC = $(SRCS) debug_timer.c
SL_SRC = serial_logger.c aq_serial.c utils.c packetLogger.c rs_msg_utils.c timespec_subtract.c

DD_SRC = dummy_device.c aq_serial.c utils.c packetLogger.c rs_msg_utils.c timespec_subtract.c
DR_SRC = dummy_reader.c aq_serial.c utils.c packetLogger.c rs_msg_utils.c timespec_subtract.c

# Build durectories
SRC_DIR := ./source
OBJ_DIR := ./build
DBG_OBJ_DIR := $(OBJ_DIR)/debug
SL_OBJ_DIR := $(OBJ_DIR)/slog
DD_OBJ_DIR := $(OBJ_DIR)/dummydevice
DR_OBJ_DIR := $(OBJ_DIR)/dummyreader

INCLUDES := -I$(SRC_DIR)

# define path for obj files per architecture
OBJ_DIR_ARMHF := $(OBJ_DIR)/armhf
OBJ_DIR_ARM64 := $(OBJ_DIR)/arm64
OBJ_DIR_AMD64 := $(OBJ_DIR)/amd64
SL_OBJ_DIR_ARMHF := $(OBJ_DIR_ARMHF)/slog
SL_OBJ_DIR_ARM64 := $(OBJ_DIR_ARM64)/slog
SL_OBJ_DIR_AMD64 := $(OBJ_DIR_AMD64)/slog

# append path to source
SRCS := $(patsubst %.c,$(SRC_DIR)/%.c,$(SRCS))
DBG_SRC := $(patsubst %.c,$(SRC_DIR)/%.c,$(DBG_SRC))
SL_SRC := $(patsubst %.c,$(SRC_DIR)/%.c,$(SL_SRC))
DD_SRC := $(patsubst %.c,$(SRC_DIR)/%.c,$(DD_SRC))
DR_SRC := $(patsubst %.c,$(SRC_DIR)/%.c,$(DR_SRC))

# append path to obj files per architecture
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DBG_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(DBG_OBJ_DIR)/%.o,$(DBG_SRC))
SL_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(SL_OBJ_DIR)/%.o,$(SL_SRC))
DD_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(DD_OBJ_DIR)/%.o,$(DD_SRC))
DR_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(DR_OBJ_DIR)/%.o,$(DR_SRC))

OBJ_FILES_ARMHF := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_ARMHF)/%.o,$(SRCS))
OBJ_FILES_ARM64 := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_ARM64)/%.o,$(SRCS))
OBJ_FILES_AMD64 := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR_AMD64)/%.o,$(SRCS))

SL_OBJ_FILES_ARMHF := $(patsubst $(SRC_DIR)/%.c,$(SL_OBJ_DIR_ARMHF)/%.o,$(SL_SRC))
SL_OBJ_FILES_ARM64 := $(patsubst $(SRC_DIR)/%.c,$(SL_OBJ_DIR_ARM64)/%.o,$(SL_SRC))
SL_OBJ_FILES_AMD64 := $(patsubst $(SRC_DIR)/%.c,$(SL_OBJ_DIR_AMD64)/%.o,$(SL_SRC))

#OBJ_FILES := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
#DBG_OBJ_FILES := $(patsubst %.c,$(DBG_OBJ_DIR)/%.o,$(DBG_SRC))
#SL_OBJ_FILES := $(patsubst %.c,$(SL_OBJ_DIR)/%.o,$(SL_SRC))

#OBJ_FILES_ARMHF := $(patsubst %.c,$(OBJ_DIR_ARMHF)/%.o,$(SRCS))
#OBJ_FILES_ARM64 := $(patsubst %.c,$(OBJ_DIR_ARM64)/%.o,$(SRCS))
#OBJ_FILES_AMD64 := $(patsubst %.c,$(OBJ_DIR_AMD64)/%.o,$(SRCS))

#SL_OBJ_FILES_ARMHF := $(patsubst %.c,$(SL_OBJ_DIR_ARMHF)/%.o,$(SL_SRC))
#SL_OBJ_FILES_ARM64 := $(patsubst %.c,$(SL_OBJ_DIR_ARM64)/%.o,$(SL_SRC))
#SL_OBJ_FILES_AMD64 := $(patsubst %.c,$(SL_OBJ_DIR_AMD64)/%.o,$(SL_SRC))


#MG_OBJ_FILES := $(patsubst %.c,$(OBJ_DIR)/%.o,$(MG_SRC))

# define the executable file
MAIN = ./release/aqualinkd
SLOG = ./release/serial_logger
DEBG = ./release/aqualinkd-debug
DDEVICE = ./release/dummydevice
DREADER = ./release/dummyreader

MAIN_ARM64 = ./release/aqualinkd-arm64
MAIN_ARMHF = ./release/aqualinkd-armhf
MAIN_AMD64 = ./release/aqualinkd-amd64

SLOG_ARM64 = ./release/serial_logger-arm64
SLOG_ARMHF = ./release/serial_logger-armhf
SLOG_AMD64 = ./release/serial_logger-amd64

#LOGR = ./release/log_reader
#PLAY = ./release/aqualinkd-player


# Rules with no targets
.PHONY: clean clean-buildfiles buildrelease release install

# Default target
.DEFAULT_GOAL := all

# Before the below works, you need to build the aqualinkd-releasebin docker for compiling.
# sudo docker build -f Dockerfile.releaseBinaries -t aqualinkd-releasebin .
# Something like below
#releasebuilddocker:
#	sudo docker build -f ./docker/Dockerfile.releaseBinaries -t aqualinkd-releasebin .
#	$(info Docker for building release binaries has been created)

release:
	sudo docker run -it --mount type=bind,source=./,target=/build aqualinkd-releasebin make buildrelease 
	$(info Binaries for release have been built)

quick:
	sudo docker run -it --mount type=bind,source=./,target=/build aqualinkd-releasebin make quickbuild 
	$(info Binaries for release have been built)

debugbinaries:
	sudo docker run -it --mount type=bind,source=./,target=/build aqualinkd-releasebin make debugbuild 
	$(info Binaries for release have been built)

runindocker:
	sudo docker run -it --mount type=bind,source=./,target=/build aqualinkd-releasebin make dockerbuildnrun 
	$(info Binaries for release have been built)

# This is run inside container Dockerfile.releaseBinariies (aqualinkd-releasebin)
buildrelease: clean armhf arm64 
	$(shell cd release && ln -s ./aqualinkd-armhf ./aqualinkd && ln -s ./serial_logger-armhf ./serial_logger)

# This is run inside container Dockerfile.releaseBinariies (aqualinkd-releasebin)
quickbuild: armhf arm64 
	$(shell cd release && [ ! -f "./aqualinkd-armhf" ] && ln -s ./aqualinkd-armhf ./aqualinkd && ln -s ./serial_logger-armhf ./serial_logger)

# This is run inside container Dockerfile.releaseBinariies (aqualinkd-releasebin)
dockerbuildnrun: ./release/aqualinkd
	$(shell ./release/aqualinkd -d -c ./realease/aqualinkd.test.conf


debugbuild: CFLAGS = $(DFLAGS)
debugbuild: armhf arm64 
	$(shell cd release && [ ! -f "./aqualinkd-armhf" ] && ln -s ./aqualinkd-armhf ./aqualinkd && ln -s ./serial_logger-armhf ./serial_logger)


# Rules to pass to make.
all:    $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled)
	$(info $(SLOG) has been compiled)

slog:	$(SLOG)
	$(info $(SLOG) has been compiled)

aqdebug: $(DEBG)
	$(info $(DEBG) has been compiled)

dummydevice:	$(DDEVICE)
	$(info $(DDEVICE) has been compiled)

dummyreader:	$(DREADER)
	$(info $(DREADER) has been compiled)

# Container, add container flag and compile
container: CFLAGS := $(CFLAGS) -D AQ_CONTAINER
container: $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled (** For Container use **))
	$(info $(SLOG) has been compiled (** For Container use **))

container-arm64: CC := $(CC_ARM64)
container-arm64: container

container-amd64: CC := $(CC_AMD64)
container-amd64: container

# armhf
armhf: CC := $(CC_ARMHF)
armhf: $(MAIN_ARMHF) $(SLOG_ARMHF)
	$(info $(MAIN_ARMHF) has been compiled)
	$(info $(SLOG_ARMHF) has been compiled)

# arm64
arm64: CC := $(CC_ARM64)
arm64: $(MAIN_ARM64) $(SLOG_ARM64)
	$(info $(MAIN_ARM64) has been compiled)
	$(info $(SLOG_ARM64) has been compiled)

# amd64
amd64: CC := $(CC_AMD64)
amd64: $(MAIN_AMD64) $(SLOG_AMD64)
	$(info $(MAIN_AMD64) has been compiled)
	$(info $(SLOG_AMD64) has been compiled)

#debug, Just change compile flags and call MAIN
debug: CFLAGS = $(DFLAGS)
debug: $(MAIN) $(SLOG)
	$(info $(MAIN) has been compiled (** DEBUG **))
	$(info $(SLOG) has been compiled (** DEBUG **))

#install: $(MAIN)
install:
	./release/install.sh from-make


# Rules to compile
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(DBG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DBG_OBJ_DIR)
	$(CC) $(DBG_CFLAGS) $(INCLUDES) -c -o $@ $<

$(SL_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(SL_OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(DD_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DD_OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(DR_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DR_OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJ_DIR_ARMHF)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(SL_OBJ_DIR_ARMHF)/%.o: $(SRC_DIR)/%.c | $(SL_OBJ_DIR_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJ_DIR_ARM64)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(SL_OBJ_DIR_ARM64)/%.o: $(SRC_DIR)/%.c | $(SL_OBJ_DIR_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJ_DIR_AMD64)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR_AMD64)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(SL_OBJ_DIR_AMD64)/%.o: $(SRC_DIR)/%.c | $(SL_OBJ_DIR_AMD64)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Rules to link
$(MAIN): $(OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS) 

$(MAIN_ARM64): $(OBJ_FILES_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS) 

$(MAIN_ARMHF): $(OBJ_FILES_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(MAIN_AMD64): $(OBJ_FILES_AMD64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS) 

$(DEBG): $(DBG_OBJ_FILES)
	$(CC) $(DBG_CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(SLOG): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG): $(SL_OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(SLOG_ARMHF): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG_ARMHF): $(SL_OBJ_FILES_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(SLOG_ARM64): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG_ARM64): $(SL_OBJ_FILES_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(SLOG_AMD64): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER
$(SLOG_AMD64): $(SL_OBJ_FILES_AMD64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(DDEVICE): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER -D DUMMY_DEVICE
$(DDEVICE): $(DD_OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(DREADER): CFLAGS := $(CFLAGS) -D SERIAL_LOGGER -D DUMMY_DEVICE -D DUMMY_READER
$(DREADER): $(DR_OBJ_FILES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

# Rules to make object directories.
$(OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(SL_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(DD_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(DR_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(DBG_OBJ_DIR):
	$(MKDIR) $(call FixPath,$@)

$(OBJ_DIR_ARMHF):
	$(MKDIR) $(call FixPath,$@)

$(SL_OBJ_DIR_ARMHF):
	$(MKDIR) $(call FixPath,$@)

$(OBJ_DIR_ARM64):
	$(MKDIR) $(call FixPath,$@)

$(SL_OBJ_DIR_ARM64):
	$(MKDIR) $(call FixPath,$@)

$(OBJ_DIR_AMD64):
	$(MKDIR) $(call FixPath,$@)

$(SL_OBJ_DIR_AMD64):
	$(MKDIR) $(call FixPath,$@)

# Clean rules

clean: clean-buildfiles
	$(RM) *.o *~ $(MAIN) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(DEBG) $(DDEVICE) $(DREADER)
	$(RM) $(wildcard *.o) $(wildcard *~) $(MAIN) $(MAIN_ARM64) $(MAIN_ARMHF) $(MAIN_AMD64) $(SLOG) $(DDEVICE) $(SLOG_ARM64) $(SLOG_ARMHF) $(SLOG_AMD64) $(MAIN_U) $(PLAY) $(PL_EXOBJ) $(LOGR) $(PLAY) $(DEBG)

clean-buildfiles:
	$(RM) $(wildcard *.o) $(wildcard *~) $(OBJ_FILES) $(DBG_OBJ_FILES) $(SL_OBJ_FILES) $(DD_OBJ_FILES) $(DR_OBJ_FILES) $(OBJ_FILES_ARMHF) $(OBJ_FILES_ARM64) $(OBJ_FILES_AMD64) $(SL_OBJ_FILES_ARMHF) $(SL_OBJ_FILES_ARM64) $(SL_OBJ_FILES_AMD64)


