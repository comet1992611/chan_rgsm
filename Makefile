#version file should contain a definition for SW_VERSION variaable
include ./version

PROJ = chan_rgsm
OBJ  = ggw8_hal.o rgsm_timers.o char_conv.o rgsm_utilities.o at.o rgsm_dao.o rgsm_sim5320.o rgsm_sim900.o rgsm_uc15.o rgsm_sms.o rgsm_call_sm.o rgsm_mdm_sm.o rgsm_at_processor.o chan_rgsm.o rgsm_manager.o rgsm_cli.o rgsm_app.o rgsm_dfu.o rgsm_ipc.o

CC = gcc
LD = gcc
STRIP = strip
RM = rm -f
CHMOD = chmod
INSTALL = install
AST_VERSION = 10800
INCS=$(shell pkg-config --cflags libusb-1.0) -I. -I/usr/include
AST_CONFIG_DIR=/etc/asterisk
AST_DATA_DIR=/var/lib/asterisk
SIM900_HEX_DOWNLOAD=sim900_hex_download.hex
SIM900_FW_IMAGE=sim900_fw_image.bin


CFLAGS  += -fgnu89-inline -Wall -Wattributes -fPIC -DAST_MODULE=\"$(PROJ)\" -D_THREAD_SAFE -O1 -DICONV_CONST=\"\" -D__MANAGER__ -D_GNU_SOURCE -DASTERISK_VERSION_NUM=$(AST_VERSION) -DRGSM_VERSION_STR=\"$(RGSM_VERSION_STR)\" -DRGSM_VERSION_DATE=\"$(RGSM_VERSION_DATE)\"
#CFLAGS  += -g -Wall -Wattributes -fPIC -DAST_MODULE=\"$(PROJ)\" -D_THREAD_SAFE -O1 -DICONV_CONST=\"\" -D__DEBUG__ -D__MANAGER__ -D_GNU_SOURCE -DASTERISK_VERSION_NUM=$(AST_VERSION) -DRGSM_VERSION_STR=\"$(RGSM_VERSION_STR)\" -DRGSM_VERSION_DATE=\"$(RGSM_VERSION_DATE)\"

#CFLAGS  += -g -Wall -Wattributes -fPIC -DAST_MODULE=\"$(PROJ)\" -D_THREAD_SAFE -O1 -DICONV_CONST=\"\" -D__DEBUG__ -D__MANAGER__ -D__APP__ -D_GNU_SOURCE -DASTERISK_VERSION_NUM=$(AST_VERSION) -DRGSM_VERSION_STR=\"$(RGSM_VERSION_STR)\" -DRGSM_VERSION_DATE=\"$(RGSM_VERSION_DATE)\"

ifdef DEBUG
	CFLAGS += -g -D__DEBUG__
else
	CFLAGS += -DNODEBUG
endif

LDFLAGS += -shared-libgcc
LIBS     = -pthread $(shell pkg-config libusb-1.0 --libs) -lrt $(shell pkg-config --libs sqlite3)

SOLINK  = -shared -Xlinker -x

.PHONY  : install

all	: clean $(PROJ).so

install	: all
#	$(STRIP) $@
	$(INSTALL) -m 755 $(PROJ).so /usr/lib/asterisk/modules/
ifeq ($(wildcard $(AST_CONFIG_DIR)/$(PROJ).conf),)
	cp ./$(PROJ).conf $(AST_CONFIG_DIR)
else
	cp ./$(PROJ).conf $(AST_CONFIG_DIR)/$(PROJ).conf.default.v$(RGSM_VERSION_STR)
endif

	mkdir -p $(AST_DATA_DIR)/firmware/rgsm
	cp -r ./firmware/* $(AST_DATA_DIR)/firmware/rgsm/

$(PROJ).so: $(OBJ)
	$(LD) $(SOLINK) $(LDFLAGS) $(OBJ) $(LIBS) -o $@
	$(CHMOD) 755 $@

.c.o	:
	$(CC) $(INCS) $(CFLAGS) -c $<

clean	:
	@$(RM) $(PROJ).so *.o

test    : clean $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

usbenum : usbenum.c
	$(CC) -o usbenum usbenum.c $(INCS) $(CFLAGS) -std=c99 $(LIBS)
	
