/*! \file
 * \author sogurtsov
 * \brief GGW8 device hardware abstraction layer implementation
 */

#include <libusb-1.0/libusb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "ggw8_hal.h"
#include "rgsm_defs.h"
#include <pthread.h>
#include <asterisk.h>
#include <asterisk/logger.h>
#include <asterisk/lock.h>
#include <asterisk/linkedlists.h>
#include <asterisk/select.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include "atomics.h"
#include <errno.h>
#include "rgsm_utilities.h"
#include "rgsm_dfu.h"

//GGW8 interface number
#define GGW8_USB_INTERFACE_NO               0

//endpoints assignment
#define AT_IN_EP                            1
#define AT_OUT_EP                           2
#define VOICE_IN_EP                         5
#define VOICE_OUT_EP                        6

#define CONTROL_TRANSFER_TIMEOUT_MS         25
#define INTERRUPT_IN_TIMEOUT_MS             1000
#define INTERRUPT_OUT_TIMEOUT_MS            25
#define SYNC_CONTROL_TRANSFERS              0
#define CONTROL_TRANSFERS_RETRY_COUNT       3

#define DEVICE_INFO_RID                     0x02
#define DEVICE_CONTROL_RID                  0x04
#define DEVICE_STATE_RID                    0x05
#define MODEM_CONTROL_RID                   0x06
#define VOICE_CONTROL_RID                   0x07
#define BAUD_RATE_RID                       0x08

#define INTERRUPT_IN_DFU_TIMEOUT_MS         10000

#define DEBUG_HAL(fmt, ...) do { \
    if (gen_config.debug_ctl.hal) \
        ast_log(LOG_DEBUG, fmt, ## __VA_ARGS__); \
} while (0)

#define ERROR_HAL(fmt, ...) ast_log(AST_LOG_ERROR, fmt, ## __VA_ARGS__)

#define AUDIO_IN_PACKETS_PER_TRANSFER       8
#define AUDIO_IN_NUM_TRANSFERS              2

#define ATTEMPT_COUNT_INTERRUPT_OUT         3

extern gen_config_t     gen_config;

//! Static members definition

//each physical device maintains own queues and monitor threads
struct ggw8_device {
    libusb_device                               *dev;
    libusb_device_handle                        *dev_handle;
    uint16_t                                    sys_id; //high byte is a bus no and low byte is an address on that bus
    ggw8_mode_t                                 mode;

//if following macro is non zero then control transfers will be synchronized
#if SYNC_CONTROL_TRANSFERS == 1
    ast_mutex_t                                 ctlep_mtx;
#endif
    ggw8_info_t                                 dev_info;

    pthread_t                                   atin_monitor_thread;
    pthread_t                                   atout_monitor_thread;
    pthread_t                                   voicein_monitor_thread;
    pthread_t                                   voiceout_monitor_thread;

    //AT FDs, where at_fds[X][0] refers the FD the hal will read/write to and at_fds[X][1] refers the FD a channel driver will read/write to
    int                                         at_fds[MAX_MODEMS_ON_BOARD][2];

    //VOICE FDs, where voive_fds[X][0] refers the FD the hal will read/write to and voice_fds[X][1] refers the FD a channel driver will read/write to
    int                                         voice_fds[MAX_MODEMS_ON_BOARD][2];

    atomic_t                                    is_alive;
    atomic_t                                    stopping;
    atomic_t                                    is_restart;
    atomic_t                                    atin_monitor_running;
};


//USB specifics constants
static const int CONTROL_REQUEST_TYPE_IN = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;

static libusb_context       *usb_context;

#define check_context(dev) { \
    if (!usb_context || !dev || !dev->dev_handle) return GGW8_ERROR_INVALID_CONTEXT; \
}

static int fd_set_blocking(int fd, int blocking) {
    /* Save the current flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) != -1;
}

static int get_string_descriptor(libusb_device_handle *dev_handle, uint8_t index, char* buf, size_t bufsize)
{
    int rc;
    if (dev_handle && bufsize) {
        rc = libusb_get_string_descriptor_ascii(dev_handle, index,(unsigned char*)buf, bufsize);
        if(rc > 0) {
            return 1;
        } else {
            ast_log(AST_LOG_ERROR, "libusb_get_string_descriptor_ascii failed: rc=%d, index=%d\n", rc, index);
        }
    }
    buf[0] = 0;
    return 0;
}

static int check_device(hw_config_t *cfg, libusb_device *device, libusb_device_handle **dev_handle) {
    struct libusb_device_descriptor info;
    libusb_device_handle            *tmp_dev_handle = NULL;

    int rc = libusb_get_device_descriptor(device, &info);

    if (rc) {
        ast_log(AST_LOG_ERROR, "libusb_get_device_descriptor failed: rc=%d\n", rc);
        return 0;
    }

    //ast_log(AST_LOG_DEBUG, "checking libusb_device=%p\n", device);

    if((info.idVendor == cfg->vid) && (info.idProduct == cfg->pid)) {
        //TODO check device here: operational state etc, add if test passed

        char buf[255];
        rc = libusb_open(device, &tmp_dev_handle);
        if(rc) {
            //open failed, mostly caused by file permissions,
            ast_log(AST_LOG_ERROR, "libusb_open failed: libusb_device=%p, rc=%d\n", device, rc);
            return 0;
        }

        //if product_string is non empty verify it
        if (cfg->product_string && strlen(cfg->product_string)) {
            if (!get_string_descriptor(tmp_dev_handle, info.iProduct, buf, sizeof(buf))) {
                goto _cleanup;
            }
            if (strcmp(buf, cfg->product_string)) {
                //product strings do not match -> not our device
                goto _cleanup;
            }
        }

        if (!get_string_descriptor(tmp_dev_handle, info.iSerialNumber, buf, sizeof(buf))) {
            ast_log(AST_LOG_ERROR, "failed to read a serial number descriptor: libusb_device=%p, rc=%d\n", device, rc);
            goto _cleanup;
        }

        //vid, pid and optionally product string matches, keep device_handle opened
        *dev_handle = tmp_dev_handle;
        return 1;
    }
    return 0;
_cleanup:
    //close tmp device_handle
    libusb_close(tmp_dev_handle);
    return 0;
}

// shortcut function for pulling feature requests of given device, synchronous
static int get_feature(struct ggw8_device *device, unsigned char* dataIn, uint8_t reportId, uint16_t maxLen)
{
    int attempt = 0;
    int ret;

    check_context(device);

    while (1) {
#if SYNC_CONTROL_TRANSFERS == 1
        ast_mutex_lock(&ctlep_mtx);
#endif
        ret = libusb_control_transfer(
				device->dev_handle,
				CONTROL_REQUEST_TYPE_IN ,
				HID_GET_REPORT,
				(HID_REPORT_TYPE_FEATURE<<8)|reportId,
				0, // interface number
				dataIn,
				maxLen, // max size
				CONTROL_TRANSFER_TIMEOUT_MS); // timeout for operation
#if SYNC_CONTROL_TRANSFERS == 1
        ast_mutex_unlock(&ctlep_mtx);
#endif
        if (ret >= 0) break;
        attempt++;
        if (attempt >= CONTROL_TRANSFERS_RETRY_COUNT) break;
    }

    if(ret < 0) {
        ast_log(AST_LOG_ERROR, "usb::get_feature() failed: rep_id=%d, code=%d, attempts=%d\n", reportId, ret, attempt);
        //negative return codes are the libusb error codes
        return ret;
    } else {
        DEBUG_HAL("usb::get_feature() succeed: rep_id=%d, bytes_requested=%d, bytes_read=%d\n", reportId, maxLen, ret);
        return GGW8_ERROR_SUCCESS;
    }
}

//shortcut function for setfeature requests, synchronous
static int set_feature(struct ggw8_device *device, unsigned char* dataOut, uint8_t reportId, uint16_t maxLen)
{
    int attempt = 0;
    int ret = 0;

    check_context(device);

    while (1) {
#if SYNC_CONTROL_TRANSFERS == 1
        ast_mutex_lock(&ctlep_mtx);
#endif
        ret = libusb_control_transfer(
				device->dev_handle,
				CONTROL_REQUEST_TYPE_OUT ,
				HID_SET_REPORT,
				(HID_REPORT_TYPE_FEATURE<<8)|reportId,
				0, // interface number
				dataOut,
				maxLen, // max size
				CONTROL_TRANSFER_TIMEOUT_MS); // timeout for operation
#if SYNC_CONTROL_TRANSFERS == 1
        ast_mutex_unlock(&ctlep_mtx);
#endif
        if (ret >= 0) break;
        attempt++;
        if (attempt >= CONTROL_TRANSFERS_RETRY_COUNT) break;
    }
    if(ret < 0) {
        ast_log(AST_LOG_ERROR, "usb::set_feature() failed: rep_id=%d, code=%d, attempts=%d\n", reportId, ret, attempt);
        return ret;
    } else {
        DEBUG_HAL("usb::set_feature() succeed: rep_id=%d, bytes_to_send=%d, bytes_sent=%d\n", reportId, maxLen, ret);
        return GGW8_ERROR_SUCCESS;
    }
}

static int interrupt_in(struct ggw8_device *device, unsigned char endpoint, unsigned char *data, int length, int *bytes_received, int timeout)
{
    check_context(device);

    int rc = libusb_interrupt_transfer(device->dev_handle,
                                       endpoint | (unsigned char)LIBUSB_ENDPOINT_IN,
                                       data,
                                       length,
                                       bytes_received,
                                       timeout);
    if (rc == 0) {
        if (*bytes_received > 0) {
            //
            //header length is 3 bytes for dfu and at transfers
            DEBUG_HAL("usb::interrupt_in() succeed: bytes_received=%d, header=%.*s, data=%.*s\n",
                      *bytes_received, 3, data, *bytes_received-3, data+3);
        }
    } else {
        if ((rc != LIBUSB_ERROR_TIMEOUT) && (rc != LIBUSB_ERROR_NO_DEVICE)) {
            ast_log(AST_LOG_ERROR, "usb::interrupt_in() failed: rc=%d\n", rc);
        }
    }
    return rc;
}

static int interrupt_out(struct ggw8_device *device, unsigned char endpoint, unsigned char *data, int length, int *bytes_sent)
{
    int rc, header_len;
    int attempt = ATTEMPT_COUNT_INTERRUPT_OUT;
    check_context(device);

    while (attempt--) {
        rc = libusb_interrupt_transfer(device->dev_handle,
                                       endpoint | (unsigned char)LIBUSB_ENDPOINT_OUT,
                                       data,
                                       length,
                                       bytes_sent,
                                       INTERRUPT_OUT_TIMEOUT_MS);
        if (rc == LIBUSB_ERROR_TIMEOUT) {
            continue;
        }
        //succeed or not LIBUSB_ERROR_TIMEOUT
        break;
    }
    if (rc == 0) {
        //
        header_len = (atomic_read(&is_dfu) != 0) ? 2 : 3;
        DEBUG_HAL("usb::interrupt_out() succeed: bytes_to_send=%d, bytes_sent=%d, header=%.*s, data=%.*s\n",
                  length, *bytes_sent, header_len, data, length-header_len, data+header_len);
    }
    else if (length != *bytes_sent) {
        ast_log(AST_LOG_ERROR, "usb::interrupt_out() failed: bytes_to_send=%d, bytes_sent=%d, data=%.*s\n", length, *bytes_sent, length, data);
    }
    else {
        ast_log(AST_LOG_ERROR, "usb::interrupt_out() failed: bytes_to_send=%d, rc=%d\n", length, rc);
    }

    return rc;
}

static void cb_iso_out(struct libusb_transfer *audio_transfer)
{
    int status = audio_transfer->iso_packet_desc[0].status;
    if (status) {
        ast_log(AST_LOG_ERROR, "iso_out: failed to complete transfer: status=%d\n", status);
    } else {
        if (gen_config.debug_ctl.hal) {
            int len = audio_transfer->iso_packet_desc[0].actual_length;
            ast_log(AST_LOG_DEBUG, "iso_out: complete transfer: actual_length=%d\n", len);
        }
    }
}

int iso_out(struct ggw8_device *device, unsigned char endpoint, unsigned char *data, int length, int max_iso_packet_size)
{
    check_context(device);

    int rc;
    unsigned char *buf = NULL;
    int max_audio_packets = 1;
    //int audio_buffer_len = max_iso_packet_size * max_audio_packets;

    struct libusb_transfer *audio_transfer;

    audio_transfer = libusb_alloc_transfer(max_audio_packets);
    if (!audio_transfer) {
        ast_log(AST_LOG_ERROR, "iso_out: out of memory\n");
        rc = ERROR_OUT_OF_MEM;
        goto cleanup_;
    }

    buf = (unsigned char*)malloc(length);
    if (!buf) {
        ast_log(AST_LOG_ERROR, "iso_out: out of memory\n");
        rc = ERROR_OUT_OF_MEM;
        goto cleanup_;
    }

    memcpy(buf, data, length);

    libusb_fill_iso_transfer(audio_transfer,        // transfer
                           device->dev_handle,         // dev_handle
                           endpoint | (unsigned char)LIBUSB_ENDPOINT_OUT,        // endpoint
                           buf,           // buffer
                           length,     // length
                           max_audio_packets,       // num_iso_packets
                           cb_iso_out,   // callback
                           0,           // user_data
                           500                       // timeout
                          );

    libusb_set_iso_packet_lengths(audio_transfer, max_iso_packet_size);

    audio_transfer->num_iso_packets = max_audio_packets;
    audio_transfer->type |= LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
    audio_transfer->flags |= (LIBUSB_TRANSFER_FREE_TRANSFER | LIBUSB_TRANSFER_FREE_BUFFER);     //free transfer and buffer on complete

    rc = libusb_submit_transfer(audio_transfer);
    if (rc) {
        ast_log(AST_LOG_ERROR, "iso_out: failed submit transfer: audio_packet length=%d\n", length);
        goto cleanup_;
    }

    DEBUG_HAL("iso_out: submit transfer: audio_packet length=%d\n", length);

cleanup_:
    if (rc) {
        free(buf);
        free(audio_transfer);
    }

    return rc;
}

#define FD_CLIENT   0
#define FD_SRV      1

//AT-IN monitor thread function
//This thread implicitely running a loop for handling libusb events
static void *atin_monitor_func(void *data) {
    struct ggw8_device *device = (struct ggw8_device*)data;
    int bytes_received;
    uint16_t data_len;
    uint8_t modem_id;
    int rc;
    unsigned char atin_buf[0x200]; //buffer for interrupt-in data
    int zlp_count = 0;

    atomic_set(&device->atin_monitor_running, 1);

    ast_log(AST_LOG_DEBUG, "atin_monitor: thread started: ggw8=%p\n", device);
    while (atomic_read(&device->atin_monitor_running)) {
        bytes_received = 0;
        rc = interrupt_in(device, AT_IN_EP,  atin_buf, sizeof(atin_buf), &bytes_received, 0);
        if (rc == GGW8_ERROR_INVALID_CONTEXT || rc == LIBUSB_ERROR_NO_DEVICE) {
            //set flag and exit thread
            atomic_set(&device->is_alive, 0);
            us_sleep(100);
            continue;
            //break;
        } else if (bytes_received == 0) {
            zlp_count++;
            if (zlp_count % 100 == 0) {
                ast_log(AST_LOG_WARNING, "atin_monitor: zlps arrived: count=%d\n", zlp_count);
            }
            continue;
        } else if (bytes_received < 3) {
            ast_log(AST_LOG_WARNING, "atin_monitor: packet too small: len=%d\n", bytes_received);
            continue;
        }

        data_len = *((uint16_t*)&atin_buf[1]);

        //! workaround a BF usb: bytes reived may be greater than data_len+3
        if (data_len+3 > bytes_received) {
            //logit
            ast_log(AST_LOG_WARNING, "atin_monitor: malformed at-in packet: bytes_received=%d, data_len=%d\n", bytes_received, data_len);
            continue;
        } else if (!data_len) {
            //zero length payload
            ast_log(AST_LOG_WARNING, "atin_monitor: zero length payload in at-in packet\n");
            continue;
        }

        modem_id = atin_buf[0];
        if (modem_id >= MAX_MODEMS_ON_BOARD) {
            ast_log(AST_LOG_WARNING, "atin_monitor: invalid modem_id=%d in at-in packet\n", modem_id);
            continue;
        }

        if (device->at_fds[modem_id][FD_SRV] == -1) {
            //ast_log(AST_LOG_WARNING, "atin_monitor: upstream fd not opened, response skipped: modem_id=%d, bytes_received=%d\n",
            //        modem_id, bytes_received);
            continue;
        }
        
        int try_count = 100;
        int ret = 0;

        do {
          try_count--;
          ret = write(device->at_fds[modem_id][FD_SRV], &atin_buf[3], data_len);
        } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) && try_count > 0);

        //write to AT-in channel in upstream direction -> demultiplex
        if (ret == -1) {
            ast_log(AST_LOG_ERROR, "atin_monitor: error writing to upstream at_fd: dev=%p, modem_id=%d, errno=%d\n",
                    device, modem_id, errno);
        }

        fsync(device->at_fds[modem_id][FD_SRV]);
    }
    ast_log(AST_LOG_DEBUG, "atin_monitor: thread stopped: ggw8=%p\n", device);
    device->atin_monitor_thread = AST_PTHREADT_NULL;
    return NULL;
}

static void atout_send_buf(struct ggw8_device *device, unsigned char *buf, int buf_len)
{
    int bytes_sent;
    int rc = interrupt_out(device, AT_OUT_EP,  buf, buf_len, &bytes_sent);
    if ((rc == 0) && (buf_len == bytes_sent)) {
        //May 21, 2013: delay removed
        //simulate delay to avoid overrun a serial interface of GW
        //us_sleep((buf_len - GGW8_AT_HEADER_SIZE)*100);
    }
    else if (rc == GGW8_ERROR_INVALID_CONTEXT || rc == LIBUSB_ERROR_NO_DEVICE) {
        //set flag and exit thread
        atomic_set(&device->is_alive, 0);
    } else {
        ast_log(AST_LOG_ERROR, "atout_monitor: failed to write AT cmd to device: rc=%d, bytes_to_send=%d, bytes_sent=%d\n",
                rc, buf_len, bytes_sent);
    }
}

//AT-out monitor thread function
static void *atout_monitor_func(void *data) {
    struct ggw8_device *device = (struct ggw8_device*)data;
    int nfds, rc, fd, indx, i, nbytes;
    unsigned char atout_buf[GGW8_ATOUT_BUFSIZE]; //must be compliant with GW firmware
    //fd_set rfds;
    ast_fdset rfds;

    char ch;
    struct timeval tv;

    ast_log(AST_LOG_DEBUG, "atout_monitor: thread started: ggw8=%p\n", device);
    while (!atomic_read(&device->stopping)) {
        nfds = 0;
        FD_ZERO(&rfds);
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            fd = device->at_fds[i][FD_SRV];
            if (fd != -1) {
                FD_SET(fd, &rfds);
                nfds = MAX(nfds, fd);
            }
        }

        if (!nfds) {
            //no powered on channels
            usleep(5000);
            continue;
        }

        tv.tv_sec = 0;
        tv.tv_usec = 5000;

        rc = ast_select(nfds + 1, &rfds, NULL, NULL, &tv);

        if (rc == -1) {
            ast_log(AST_LOG_ERROR, "atout_monitor: select fd error: rc=%d\n", errno);
            continue;
        }

        //note: some descriptors may appear in signalling state even a timeout occured (rc = 0)
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {

            fd = device->at_fds[i][FD_SRV];
            if (fd != -1 && FD_ISSET(fd, &rfds)) {
                indx = GGW8_AT_HEADER_SIZE;
                nbytes = 0;
                while (1) {
                    rc = read(fd, &ch, 1);
                    if (rc == -1) {
                        if (errno != EAGAIN) {
                            ast_log(AST_LOG_ERROR, "atout_monitor: error reading char from at_fd=%d: err=%s, nbytes=%d\n", fd, strerror(errno), nbytes);
                            nbytes = 0;
                        }
                        break;
                    } else if (rc == 0) {
                        //no more data to read
                        break;
                    }

                    *(atout_buf+indx) = ch;
                    indx++;
                    nbytes++;

                    if (nbytes == GGW8_ATOUT_PAYLOADSIZE) break;
                }
                //send over usb
                if (nbytes) {
                    DEBUG_HAL("atout_monitor: AT cmd read from fd=%d: nbytes=%d\n", fd, nbytes);
                    atout_buf[0] = i;
                    *((uint16_t*)&atout_buf[1]) = nbytes;
                    nbytes += GGW8_AT_HEADER_SIZE;
                    atout_send_buf(device, atout_buf, nbytes);
                }

                if (!atomic_read(&device->is_alive)) break;
            }
        }
    }
    ast_log(AST_LOG_DEBUG, "atout_monitor: thread stopped: ggw8=%p\n", device);
    device->atout_monitor_thread = AST_PTHREADT_NULL;
    return NULL;
}

struct iso_in_user_data {
    struct ggw8_device *device;
    int transfer_index;
    struct libusb_transfer *transfer;
    atomic_t transfer_active;
    int cnt;
};

static void cb_iso_in(struct libusb_transfer *transfer)
{
    //struct ggw8_device *device = (struct ggw8_device *)transfer->user_data;
    struct iso_in_user_data *user_data;
    int i, rc;
    const uint8_t *data;
    rgsm_voice_frame_header_t *hdr_ptr;
    int bytes_received;

    user_data = (struct iso_in_user_data*)transfer->user_data;

    if (atomic_read(&user_data->device->stopping)) {
        ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d due to stopping\n", user_data->transfer_index);
        goto finish_voicein_transfer_;
    }
    else if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
        ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d status=NO_DEVICE\n", user_data->transfer_index);
        atomic_set(&user_data->device->is_alive, 0);
        goto finish_voicein_transfer_;
    }
    else if (transfer->status == LIBUSB_TRANSFER_CANCELLED  ) {
        ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d status=TRANSFER_CANCELLED\n", user_data->transfer_index);
        goto finish_voicein_transfer_;
    }
    else if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        ast_log(LOG_WARNING, "iso_in: transfer completed with error status=%d\n", transfer->status);
        goto resubmit_;
    }

    //iterate over transfer packets
    for (i = 0; i < AUDIO_IN_PACKETS_PER_TRANSFER; i++) {
        //
        if (atomic_read(&user_data->device->stopping)) {
            ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d due to stopping\n", user_data->transfer_index);
            goto finish_voicein_transfer_;
        }

        //get status for each packet
        rc = transfer->iso_packet_desc[i].status;

        if (rc == LIBUSB_TRANSFER_COMPLETED) {
            //
            bytes_received = transfer->iso_packet_desc[i].actual_length;
            if (!bytes_received) continue;

            hdr_ptr = (rgsm_voice_frame_header_t*)libusb_get_iso_packet_buffer_simple(transfer, i);
            data = (unsigned char*)hdr_ptr + sizeof(rgsm_voice_frame_header_t);

            DEBUG_HAL("iso_in packet#=%d.%d, actual_length=%d, mdm_id=%d, data_len=%u, data=%.2x%.2x%.2x%.2x%.2x...\n",
                      user_data->transfer_index, i, bytes_received,
                      hdr_ptr->modem_id, hdr_ptr->data_len,
                      data[0], data[1], data[2], data[3], data[4]);

            if (bytes_received < sizeof(rgsm_voice_frame_header_t)) {
                if (bytes_received != 0) {
                    ast_log(LOG_WARNING, "iso_in: in-packet too small: len=%d\n", bytes_received);
                }
                continue;
            }

            if ((hdr_ptr->data_len + sizeof(rgsm_voice_frame_header_t)) != bytes_received) {
                //logit
                ast_log(AST_LOG_WARNING, "iso_in: malformed in-packet: bytes_received=%d, expected=%d\n",
                        bytes_received, hdr_ptr->data_len + sizeof(rgsm_voice_frame_header_t));
                continue;
            }

            if (!hdr_ptr->data_len) {
                //zero length payload
                ast_log(AST_LOG_WARNING, "iso_in: zero length payload in in-packet\n");
                continue;
            }

            if (hdr_ptr->modem_id >= MAX_MODEMS_ON_BOARD) {
                ast_log(AST_LOG_WARNING, "iso_in: invalid modem_id=%d in in-packet\n", hdr_ptr->modem_id);
                continue;
            }

            if (user_data->device->voice_fds[hdr_ptr->modem_id][FD_SRV] == -1) {
                ast_log(AST_LOG_WARNING, "iso_in: upstream voice_fd not open: modem_id=%d\n", hdr_ptr->modem_id);
                continue;
            }

            //write payload to audio-in channel in upstream direction -> demultiplex
            int try_count = 100;
            int ret = 0;

            do {
              try_count--;
              ret = write(user_data->device->voice_fds[hdr_ptr->modem_id][FD_SRV], data, hdr_ptr->data_len);
            } while (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) && try_count > 0);

//            ast_log(AST_LOG_ERROR, "iso_in: first 10 bytes of (%d packet size): 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", hdr_ptr->data_len, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);

            if ( ret == -1) {
                ast_log(AST_LOG_ERROR, "iso_in: error writing to upstread voice_fd: modem_id=%d, errno=%d\n", hdr_ptr->modem_id, errno);
            } else {
                //fsync(user_data->device->voice_fds[hdr_ptr->modem_id][FD_SRV]);
            }
        }
/*
        else {
            //skip EXDEV - it's analogue of timeout
            if (atomic_read(&user_data->device->stopping)) {
                ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d due to stopping\n", user_data->transfer_index);
                goto finish_voicein_transfers_;
            }
            else if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
                ast_log(LOG_DEBUG, "iso_in: finishing transfer#=#d status=NO_DEVICE\n", user_data->transfer_index);
                goto finish_voicein_transfers_;
            }
            else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
                ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d status=TRANSFER_CANCELLED\n", user_data->transfer_index);
                goto finish_voicein_transfers_;
            }
            else if (rc != -EXDEV) {
                //ast_log(LOG_DEBUG, "iso_in: packet transfer error: status=%d\n", rc);
            }
        }
*/
    }

resubmit_:
    //re-submit transfer
    rc = libusb_submit_transfer(transfer);
    if (rc == LIBUSB_ERROR_NO_DEVICE) {
        ast_log(LOG_DEBUG, "iso_in: finishing transfer#=%d due to status=NO_DEVICE\n", user_data->transfer_index);
        atomic_set(&user_data->device->is_alive, 0);
        goto finish_voicein_transfer_;
    } else if (rc) {
        ast_log(AST_LOG_ERROR, "iso_in: failed re-submit transfer#=%d: rc=%d\n", rc, user_data->transfer_index);
        us_sleep(5000);
        if (atomic_read(&user_data->device->stopping)) {
            goto finish_voicein_transfer_;
        }
        goto resubmit_;
    } else {
//        DEBUG_HAL("iso_in: transfer resubmitted: type=%d, flags=%d, buf=%p, num_iso_packets=%d, status=%d, endpoint=%d, timeout=%d\n",
//                  transfer->type, transfer->flags, transfer->buffer, transfer->num_iso_packets, transfer->status, transfer->endpoint, transfer->timeout);
    }

    return;

finish_voicein_transfer_:
    //this won't re-submit a transfer, mark transfer inactive
    atomic_set(&user_data->transfer_active, 0);
    return;
}

static struct libusb_transfer *request_audio_in_transfer(struct iso_in_user_data *user_data)
{
    int rc;
    int voicein_buf_len; //length of dynamically allocated buffer
    unsigned char *voicein_buf = NULL;
    int max_iso_packet_size;

    struct libusb_transfer *transfer;

    transfer = libusb_alloc_transfer(AUDIO_IN_PACKETS_PER_TRANSFER);

    if (!transfer) {
        ast_log(AST_LOG_ERROR, "voicein-monitor: out of memory\n");
        return NULL;
    }

    max_iso_packet_size = libusb_get_max_iso_packet_size(user_data->device->dev, VOICE_IN_EP | (unsigned char)LIBUSB_ENDPOINT_IN);
    voicein_buf_len = max_iso_packet_size*AUDIO_IN_PACKETS_PER_TRANSFER;

    voicein_buf = (unsigned char*)malloc(voicein_buf_len);

    if (!voicein_buf) {
        ast_log(AST_LOG_ERROR, "voicein-monitor: out of memory\n");
        libusb_free_transfer(transfer);
        return NULL;
    }

    libusb_fill_iso_transfer(transfer,        // transfer
                           user_data->device->dev_handle,         // dev_handle
                           VOICE_IN_EP | (unsigned char)LIBUSB_ENDPOINT_IN,        // endpoint
                           voicein_buf,         // buffer
                           voicein_buf_len,     // length
                           AUDIO_IN_PACKETS_PER_TRANSFER,    // num_iso_packets
                           cb_iso_in,           // callback
                           user_data,              // user_data
                           0                    // timeout - wait inifinitelly
                          );

    libusb_set_iso_packet_lengths(transfer, max_iso_packet_size);

    transfer->num_iso_packets = AUDIO_IN_PACKETS_PER_TRANSFER;
    transfer->type |= LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
    transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER;     //free buffer during libusb_free_transfer()

    rc = libusb_submit_transfer(transfer);
    if (rc) {
        ast_log(AST_LOG_ERROR, "voicein-monitor: failed submit transfer: rc=%d\n", rc);
        libusb_free_transfer(transfer);
        return NULL;
    } else {
        ast_log(AST_LOG_DEBUG, "voicein-monitor: allocated iso-in transfer#=%d\n", user_data->transfer_index);
    }

    return transfer;
}

//VOICE-IN monitor thread function
static void *voicein_monitor_func(void *data)
{
    struct ggw8_device *device = (struct ggw8_device*)data;
    int i, rc;
    struct iso_in_user_data user_data[AUDIO_IN_NUM_TRANSFERS];

    ast_log(AST_LOG_DEBUG, "voicein-monitor: thread started: ggw8=%p\n", device);

    for (i = 0; i < AUDIO_IN_NUM_TRANSFERS; i++) {
        user_data[i].transfer_index = i;
        user_data[i].device = device;
        //allocate and submit transfer
        if ((user_data[i].transfer = request_audio_in_transfer(&user_data[i]))) {
            atomic_set(&user_data[i].transfer_active, 1);
        }
    }

    while (!atomic_read(&device->stopping) /*&& device->audioin_transfer*/) {
        us_sleep(10000);
    }

    ast_log(AST_LOG_DEBUG, "voicein-monitor: cancelling transfers, ggw8=%p\n", device);
    //cancel transfers
    for (i = 0; i < AUDIO_IN_NUM_TRANSFERS; i++) {
        //assign a counter to force the transfer deallocation - 100*100us = 10ms
        user_data[i].cnt = 100;
        if (user_data[i].transfer) {
            rc = libusb_cancel_transfer(user_data[i].transfer);
            if (rc == 0) continue;
            //transfer already cancelled or complete or other error occurs
            atomic_set(&user_data[i].transfer_active, 0);
        }
    }

    ast_log(AST_LOG_DEBUG, "voicein-monitor: waiting transfers complete, ggw8=%p\n", device);

wait_:
    us_sleep(100);
    //wait untill all transfers cancel and destroy them, max time to wait is 10ms
    for (i = 0; i < AUDIO_IN_NUM_TRANSFERS; i++) {
        if (user_data[i].transfer) {
            if (atomic_read(&user_data[i].transfer_active) && user_data[i].cnt--) {
                goto wait_;
            }
            ast_log(LOG_DEBUG, "voicein-monitor: transfer#=%d deallocated\n", user_data->transfer_index);
            libusb_free_transfer(user_data[i].transfer);
            user_data[i].transfer = NULL;
        }
    }

    ast_log(AST_LOG_DEBUG, "voicein-monitor: thread stopped: ggw8=%p\n", device);
    device->voicein_monitor_thread = AST_PTHREADT_NULL;
    return NULL;
}

//Voice-out monitor thread function
static void *voiceout_monitor_func(void *data) {
    struct ggw8_device *device = (struct ggw8_device*)data;
    int nfds, rc, fd, indx, i, nbytes;
    //int moredata;

    //fd_set rfds;
    ast_fdset rfds;

    int max_iso_packet_size = libusb_get_max_iso_packet_size(device->dev, VOICE_OUT_EP | (unsigned char)LIBUSB_ENDPOINT_OUT);
    unsigned char *voiceout_buf = (unsigned char*)malloc(max_iso_packet_size);
    rgsm_voice_frame_header_t *hdr = (rgsm_voice_frame_header_t*)voiceout_buf;
    int max_payload_bytes = max_iso_packet_size - sizeof(rgsm_voice_frame_header_t);

    char ch;
    struct timeval tv;

    ast_log(AST_LOG_DEBUG, "voiceout-monitor: thread started: ggw8=%p, iso_packet_size=%d\n", device, max_iso_packet_size);
    while (!atomic_read(&device->stopping)) {
        nfds = 0;
        FD_ZERO(&rfds);
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {
            fd = device->voice_fds[i][FD_SRV];
            if (fd != -1) {
                FD_SET(fd, &rfds);
                nfds = MAX(nfds, fd);
            }
        }

        if (!nfds) {
            us_sleep(1000);
            continue;
        }

        tv.tv_sec = 0;
        tv.tv_usec = 1000;

        rc = ast_select(nfds + 1, &rfds, NULL, NULL, &tv);
        if (rc == -1) {
            ast_log(AST_LOG_ERROR, "voiceout-monitor: select fd error: rc=%d\n", errno);
            continue;
        } else if (rc == 0) {
            //there are no readiness descriptors
            continue;
        }

        //note: some descriptors may appear in signalling state even a timeout occured (rc = 0)
        for (i = 0; i < MAX_MODEMS_ON_BOARD; i++) {

            fd = device->voice_fds[i][FD_SRV];
            if (fd == -1 || !FD_ISSET(fd, &rfds)) continue;
//more_data_:
//            moredata = 0;
            indx = sizeof(rgsm_voice_frame_header_t);
            nbytes = 0;
            while (1) {
                rc = read(fd, &ch, 1);
                if (rc == -1) {
                    if (errno != EAGAIN) {
                        ast_log(AST_LOG_ERROR, "voiceout-monitor: error reading char from voice_fd: err=%s, nbytes=%d\n", strerror(errno), nbytes);
                        nbytes = 0;
                    }
                    break;
                } else if (rc == 0) {
                    //no more data to read
                    break;
                }

                *(voiceout_buf+indx) = ch;
                indx++;
                nbytes++;

                if (nbytes == max_payload_bytes) {
//                    more_data = 1;
                    break;
                }
            }
            //send over usb
            if (nbytes) {
                DEBUG_HAL("voiceout-monitor: voice packet read from fd: nbytes=%d\n", nbytes);
                hdr->modem_id = i;
                hdr->data_len = nbytes;
                nbytes += sizeof(rgsm_voice_frame_header_t);

                rc = iso_out(device, VOICE_OUT_EP,  voiceout_buf, nbytes, max_iso_packet_size);
                if (rc == 0) {
                    //DEBUG_HAL("voiceout_monitor: packet submitted to device: nbytes=%d\n", nbytes);
                }
                else if (rc == GGW8_ERROR_INVALID_CONTEXT || rc == LIBUSB_ERROR_NO_DEVICE) {
                    atomic_set(&device->is_alive, 0);
                    break;
                } else {
                    ast_log(AST_LOG_ERROR, "voiceout-monitor: failed to submit packet to device: rc=%d, bytes_to_send=%d\n",
                            rc, nbytes);
                }
            }
//            if (moredata) goto more_data_;

        }
    }
    free(voiceout_buf);
    ast_log(AST_LOG_DEBUG, "voiceout-monitor: thread stopped: ggw8=%p\n", device);
    device->voiceout_monitor_thread = AST_PTHREADT_NULL;
    return NULL;
}


//! Interface functions implementation
int ggw8_init_context() {
    if (usb_context) {
        return GGW8_ERROR_ALREADY_OPEN;
    }

    int rc = libusb_init(&usb_context);

    if (rc) {
        ast_log(AST_LOG_ERROR, "libusb_init failed: rc=%d\n", rc);
    }

    return rc;
}

static int ggw8_retrieve_device_info(struct ggw8_device *dev, ggw8_info_t *info) {
    return get_feature(dev, (unsigned char*)info, DEVICE_INFO_RID, sizeof(*info));
}

int ggw8_discover_devices(hw_config_t *cfg, ggw8_isnew_device_callback_t *cb_isnew,
                          ggw8_add_device_callback_t *cb_add)
{
    libusb_device                   **devs = NULL;
    libusb_device                   *usb_device;
    libusb_device_handle            *usb_dev_handle;
    struct ggw8_device              *ggw8_device;
    int                             dev_cnt = 0;
    int                             j, k, rc;
    uint8_t                         dev_status;
    uint16_t                        sys_id;

    if (!usb_context) {
        return -1;
    }

    //holding number of devices in list
    ssize_t usb_cnt = libusb_get_device_list(usb_context, &devs); //get the list of devices
    if(usb_cnt < 0) {
        ast_log(AST_LOG_ERROR, "libusb_get_device_list failed: rc=%d\n", usb_cnt);
        return -1;
    }

    //ast_log(AST_LOG_DEBUG, "<<< Discovering ggw8 devices: usb_count=%d\n", usb_cnt);


    ssize_t i; //for iterating through the list
    for(i = 0; i < usb_cnt; i++) {
        usb_device = devs[i];

        //check here for device of interest
        if (!check_device(cfg, usb_device, &usb_dev_handle)) continue;

        sys_id = (libusb_get_bus_number(usb_device) << 8) | libusb_get_device_address(usb_device);

        if (!cb_isnew(sys_id)) {
            libusb_close(usb_dev_handle);
            continue;
        }

        //find out if kernel driver is attached and detach it if so
        if(libusb_kernel_driver_active(usb_dev_handle, 0) == 1) {
            libusb_detach_kernel_driver(usb_dev_handle, 0);
        }
        //claim interface 0 (the first) of device
        rc = libusb_claim_interface(usb_dev_handle, GGW8_USB_INTERFACE_NO);
        if(rc < 0) {
            ast_log(AST_LOG_ERROR, "libusb_claim_interface failed: rc=%d\n", rc);
            libusb_close(usb_dev_handle);
            continue;
        }

        ggw8_device = ast_calloc(1, sizeof(struct ggw8_device));

        atomic_set(&ggw8_device->is_restart, 0);
        atomic_set(&ggw8_device->atin_monitor_running, 0);
        ggw8_device->mode = NON_OPERABLE;

        //init with defaults
        ggw8_device->voicein_monitor_thread = AST_PTHREADT_NULL;
        ggw8_device->voiceout_monitor_thread = AST_PTHREADT_NULL;
        ggw8_device->atin_monitor_thread = AST_PTHREADT_NULL;
        ggw8_device->atout_monitor_thread = AST_PTHREADT_NULL;
        //
        for (j=0; j < MAX_MODEMS_ON_BOARD; j++) {
            ggw8_device->at_fds[j][FD_CLIENT] = -1;
            ggw8_device->at_fds[j][FD_SRV] = -1;
            ggw8_device->voice_fds[j][FD_CLIENT] = -1;
            ggw8_device->voice_fds[j][FD_SRV] = -1;
        }

        ast_log(AST_LOG_DEBUG, "Found GGW8 device on libusb_device=%p. Created new instance ggw8=%p\n", usb_device, ggw8_device);

        //store references on libusb objects
        ggw8_device->dev = usb_device;
        ggw8_device->dev_handle = usb_dev_handle;
        ggw8_device->sys_id = sys_id;

#if SYNC_CONTROL_TRANSFERS == 1
        ast_mutex_init(&ggw8_device->ctlep_mtx);
#endif
        if (atomic_read(&is_dfu)) {
            //dfu requested -> send special control
            ast_log(AST_LOG_DEBUG, "Switch device to DFU: ggw8=%p\n", ggw8_device);
            if ((rc = ggw8_device_ctl(ggw8_device, GGW8_CTL_DFU))) {
                ast_log(AST_LOG_ERROR, "Failed to send DFU ctl: ggw8=%p, rc=%d\n", ggw8_device, rc);
                goto cleanup_dev_;
            }
        } else {
            ast_log(AST_LOG_DEBUG, "Soft reset device and poll its orerability: ggw8=%p\n", ggw8_device);
            if ((rc = ggw8_device_ctl(ggw8_device, GGW8_CTL_SOFTRESET))) {
                ast_log(AST_LOG_ERROR, "Failed to softreset GGW8: ggw8=%p, rc=%d\n", ggw8_device, rc);
                goto cleanup_dev_;
            }
        }


        us_sleep(200000);

        //! query device state until it becomes operable or timeout elapses
        k = 20;
        dev_status = GGW8_STATUS_NONOPERABLE;
        while (k--) {
            rc = ggw8_get_device_status(ggw8_device, &dev_status);
            if (rc) {
                if (rc != LIBUSB_ERROR_TIMEOUT) goto cleanup_dev_;
            } else {
                if (atomic_read(&is_dfu)) {
                    if (dev_status == GGW8_STATUS_DFUOPERABLE) {
                        ggw8_device->mode = DFU;
                        goto device_operable_;
                    }
                } else {
                    if (dev_status == GGW8_STATUS_OPERABLE) {
                        ggw8_device->mode = GSM;
                        goto device_operable_;
                    }
                    else if (dev_status == GGW8_STATUS_DFUOPERABLE) {
                        ggw8_device->mode = DFU;
                        goto device_operable_;
                    }
                }

            }
            us_sleep(100000); //sleep total 20 x 100ms = 2 sec
        }
        goto cleanup_dev_;

device_operable_:
        //retrieve device info to know which modems are present on board
        if (ggw8_retrieve_device_info(ggw8_device, &ggw8_device->dev_info)) {
            ast_log(AST_LOG_ERROR, "Failed to query GGW8 info: ggw8=%p\n", ggw8_device);
            goto cleanup_dev_;
        }

//ggw8_device->dev_info.gsm_presence = 0x01;

        ast_log(AST_LOG_DEBUG, "GGW8 info queried: ggw8=%p, sn='%s', fw='%s', modem_present=0x%.2x, supported_codecs=0x%.4x\n",
            ggw8_device, ggw8_device->dev_info.serial_number, ggw8_device->dev_info.fw_version, ggw8_device->dev_info.gsm_presence,
            ggw8_device->dev_info.supported_codecs);

        if (ggw8_device->mode == GSM) {
            //<<< start of monitor threads, valid only for non dfu
            //atin_monitor_thread receives AT data from physical devic
            //if (pthread_create(&ggw8_device->atin_monitor_thread, NULL, atin_monitor_func, ggw8_device) < 0) {
            if (ast_pthread_create_background(&ggw8_device->atin_monitor_thread, NULL, atin_monitor_func, ggw8_device) < 0) {
                ast_log(AST_LOG_ERROR, "Unable to create atin_monitor_thread: ggw8=%p\n", ggw8_device);
                goto cleanup_dev_;
            }

            //atout_monitor_thread receives AT data from channel driver and convey them to phisical device
            //if (pthread_create(&ggw8_device->atout_monitor_thread, NULL, atout_monitor_func, ggw8_device) < 0) {
            if (ast_pthread_create_background(&ggw8_device->atout_monitor_thread, NULL, atout_monitor_func, ggw8_device) < 0) {
                ast_log(AST_LOG_ERROR, "Unable to create atout_monitor_thread: ggw8=%p\n", ggw8_device);
                goto cleanup_dev_;
            }
            //voicein_monitor_thread receives voice data from physical device
            if (ast_pthread_create_background(&ggw8_device->voicein_monitor_thread, NULL, voicein_monitor_func, ggw8_device) < 0) {
                ast_log(AST_LOG_ERROR, "Unable to create voicein_monitor_thread: ggw8=%p\n", ggw8_device);
                goto cleanup_dev_;
            }
            //atout_monitor_thread receives AT data from channel driver and convey them to phisical device
            if (ast_pthread_create_background(&ggw8_device->voiceout_monitor_thread, NULL, voiceout_monitor_func, ggw8_device) < 0) {
                ast_log(AST_LOG_ERROR, "Unable to create voiceout_monitor_thread: ggw8=%p\n", ggw8_device);
                goto cleanup_dev_;
            }
            //>>> end of monitor threads creation
        }

        atomic_set(&ggw8_device->is_alive, 1);

        ast_log(AST_LOG_DEBUG, "GGW8 hal init complete: ggw8=%p\n", ggw8_device);
        //from here a device lifetime will be controlled by channel driver

        if (!cb_add(cfg, ggw8_device)) {
            dev_cnt++;
        } else {
            goto cleanup_dev_;
        }

        us_sleep(50000);
        //end of device, iterate next
        continue;

cleanup_dev_:
        ast_log(AST_LOG_ERROR, "Device not operable or malfunctioning -> release device ggw8=%p\n", ggw8_device);
        ggw8_close_device(ggw8_device);
        //end of device, itereate next
    }

    //ast_log(AST_LOG_DEBUG, ">>> Total ggw8 devices discovered: count=%d\n", dev_cnt);
    if (devs) libusb_free_device_list(devs, 1); //free the list
    return dev_cnt;
}

/*!
 * \brief
 * Destroy the context
 */
void ggw8_close_context() {
    if (usb_context) {
        libusb_exit(usb_context);
        usb_context = NULL;
    }
}

void ggw8_restart_device(struct ggw8_device *dev)
{
    atomic_set(&dev->is_restart, 1);
}

void ggw8_close_device(struct ggw8_device *dev) {
    int i;
    if (!dev) return;

    ast_log(AST_LOG_DEBUG, "Device closing: ggw8=%p\n", dev);
    atomic_set(&dev->stopping, 1);

    //wait monitor threads exit
    while ((dev->voicein_monitor_thread != AST_PTHREADT_NULL)
           || (dev->atout_monitor_thread != AST_PTHREADT_NULL)
           || (dev->voiceout_monitor_thread != AST_PTHREADT_NULL))
    {
        us_sleep(1000);
    }

    //close device handle to allow atin_monitor_thread return from libusb functions that wait response from devices infinitelly
    if (dev->dev_handle) {

        libusb_release_interface(dev->dev_handle, GGW8_USB_INTERFACE_NO);

        //attach kernel driver
        libusb_attach_kernel_driver(dev->dev_handle, GGW8_USB_INTERFACE_NO);

        libusb_close(dev->dev_handle);
        dev->dev_handle = NULL;
        ast_log(AST_LOG_DEBUG, "Device handle destroyed: ggw8=%p\n", dev);
    }

    //May 22, 2013: fix for Bz1930 - Unloading a chan_rgsm.so sometime crasghes the asterisk
    //atin_motitor_thread implicitly provies a loop to handle libusb events -> close it after other libusb awrae threads exit
    ast_log(AST_LOG_DEBUG, "Stop atin_monitor signal sent: ggw8=%p\n", dev);
    atomic_set(&dev->atin_monitor_running, 0);
    while (dev->atin_monitor_thread != AST_PTHREADT_NULL)
    {
        us_sleep(1000);
    }


#if SYNC_CONTROL_TRANSFERS == 1
    ast_mutex_destroy(&dev->ctlep_mtx);
#endif

    //close at and voice pipes, they may be not opened yet
    for (i=0; i < MAX_MODEMS_ON_BOARD; i++) {
        ggw8_close_at(dev, i);
        ggw8_close_voice(dev, i);
    }

    ast_log(AST_LOG_DEBUG, "Device closed: ggw8=%p\n", dev);

    ast_free(dev);
}

/*!
 * \brief
 * Queries GGW8 device status, \see GGW8_STATUS_XXX constants
 * \param status the
 * \return 0 on success or non zero return code on error
 */
int ggw8_get_device_status(struct ggw8_device *dev, uint8_t *status) {
    int rc = get_feature(dev, status, DEVICE_STATE_RID, sizeof(*status));
    if (!rc) {
        DEBUG_HAL("Read status: ggw8=%p, status=0x%.2x\n", dev, *status);
    } else {
        ast_log(AST_LOG_ERROR, "Failed to read status: ggw8=%p, rc=%d\n", dev, rc);
    }
    return rc;
}

ggw8_mode_t ggw8_get_device_mode(struct ggw8_device *dev)
{
    return dev->mode;
}

inline ggw8_info_t* ggw8_get_device_info(struct ggw8_device *dev)
{
    return &dev->dev_info;
}

inline uint16_t ggw8_get_device_sysid(struct ggw8_device *dev)
{
    return dev->sys_id;
}

inline int ggw8_is_alive(struct ggw8_device *dev)
{
    return atomic_read(&dev->is_alive);
}

inline int ggw8_is_restart(struct ggw8_device *dev)
{
    return atomic_read(&dev->is_restart);
}

/*!
 * \brief
 * Send a control code to GGW8 device
 * \param ctl_code the control code, \see GGW8_CTL_XXX constants
 * \return 0 on success or non zero return code on error
 */
int ggw8_device_ctl(struct ggw8_device *dev, uint8_t ctl_code) {
    ast_log(AST_LOG_DEBUG, "Send device Ctrl: ctrl_code=%d\n", ctl_code);
    return set_feature(dev, (unsigned char*)&ctl_code, DEVICE_CONTROL_RID, sizeof(ctl_code));
}

/*!
 * \brief
 * Send a control code to GSM modem
 * \param modem_id the id of modem to send ctl code to
 * \param ctl_code the control code, \see GGW8_CTL_MODEM_XXX constants
 * \return 0 on success or non zero return code on error
 *
 * \note The modems on GGW8 board are identified by their index between 0 to 7
 */
int ggw8_modem_ctl(struct ggw8_device *dev, uint8_t modem_id, uint8_t ctl_code) {
    unsigned char data[2];
    data[0] = modem_id;
    data[1] = ctl_code;
    ast_log(AST_LOG_DEBUG, "Send modem Ctrl: modem_id=%d, ctrl_code=%d\n", modem_id, ctl_code);
    return set_feature(dev, data, MODEM_CONTROL_RID, sizeof(data));
}

int ggw8_voice_ctl(struct ggw8_device *dev, uint8_t modem_id, uint16_t format, uint16_t jbsize)
{
    unsigned char data[5];
    data[0] = modem_id;
    *(uint16_t*)(&data[1]) = format;
    *(uint16_t*)(&data[3]) = jbsize;
    ast_log(AST_LOG_DEBUG, "Send voice Ctrl: modem_id=%d, format=%u, jbsize=%u\n", modem_id, format, jbsize);
    return set_feature(dev, data, VOICE_CONTROL_RID, sizeof(data));
}

int ggw8_baudrate_ctl(struct ggw8_device *dev, uint8_t modem_id, ggw8_baudrate_t rate)
{
    unsigned char data[2];
    data[0] = modem_id;
    data[1] = (unsigned char)rate;
    ast_log(AST_LOG_DEBUG, "Send baudrate Ctrl: modem_id=%d, baudrate=%d\n", modem_id, rate);
    return set_feature(dev, data, BAUD_RATE_RID, sizeof(data));
}

//! AT interface

//int ggw8_get_at_fd(struct ggw8_device *dev, uint8_t modem_id)
//{
//    return dev->at_fds[modem_id][FD_CLIENT];
//}

int ggw8_open_at(struct ggw8_device *dev, uint8_t modem_id)
{
    //ast_log(AST_LOG_DEBUG, "AT pipe opening: ggw8=%p, modem_id=%d\n", dev, modem_id);

    if (modem_id >= MAX_MODEMS_ON_BOARD) {
        ast_log(AST_LOG_ERROR, "Invalid modem_id: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    if (!(dev->dev_info.gsm_presence & (1 << modem_id)))
    {
        ast_log(AST_LOG_ERROR, "Attempt to open AT pipe for non present modem: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    if (dev->at_fds[modem_id][FD_CLIENT] != -1) {
        ast_log(AST_LOG_DEBUG, "AT pipe already open: ggw8=%p, modem_id=%d, at_fd=%d\n", dev, modem_id, dev->at_fds[modem_id][FD_CLIENT]);
        return dev->at_fds[modem_id][FD_CLIENT];
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, dev->at_fds[modem_id])) {
        ast_log(AST_LOG_ERROR, "Failed to open AT pipe: ggw8=%p, modem_id=%d, errno=%d\n", dev, modem_id, errno);
        return -1;
    }

    //set file descriptors non-blocking
    fd_set_blocking(dev->at_fds[modem_id][FD_CLIENT], 0);
    fd_set_blocking(dev->at_fds[modem_id][FD_SRV], 0);

    ast_log(AST_LOG_DEBUG, "AT pipe open: ggw8=%p, modem_id=%d, at_fd=%d\n", dev, modem_id, dev->at_fds[modem_id][FD_CLIENT]);
    return dev->at_fds[modem_id][FD_CLIENT];
}

int ggw8_close_at(struct ggw8_device *dev, uint8_t modem_id)
{
    //ast_log(AST_LOG_DEBUG, "AT pipe closing: ggw8=%p, modem_id=%d\n", dev, modem_id);

    if (modem_id >= MAX_MODEMS_ON_BOARD) {
        ast_log(AST_LOG_ERROR, "Invalid modem_id: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    int ret = 0;

    if (dev->at_fds[modem_id][FD_SRV] != -1) {
        ret = close(dev->at_fds[modem_id][FD_SRV]);
        dev->at_fds[modem_id][FD_SRV] = -1;
        ret = 1;
    }

    if (dev->at_fds[modem_id][FD_CLIENT] != -1) {
        ret |= close(dev->at_fds[modem_id][FD_CLIENT]);
        dev->at_fds[modem_id][FD_CLIENT] = -1;
        ret = 1;
    }
    if (ret) ast_log(AST_LOG_DEBUG, "AT pipe close: ggw8=%p, modem_id=%d\n", dev, modem_id);

    return 0;
}


//! VOICE interface
//int ggw8_get_voice_fd(struct ggw8_device *dev, uint8_t modem_id)
//{
//    return dev->voice_fds[modem_id][FD_CLIENT];
//}

int ggw8_open_voice(struct ggw8_device *dev, uint8_t modem_id, uint16_t format, uint16_t jbsize)
{
    //ast_log(AST_LOG_DEBUG, "voice pipe opening: ggw8=%p, modem_id=%d\n", dev, modem_id);

    if (modem_id >= MAX_MODEMS_ON_BOARD) {
        ast_log(AST_LOG_ERROR, "Invalid modem_id: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    if (!(dev->dev_info.gsm_presence & (1 << modem_id)))
    {
        ast_log(AST_LOG_ERROR, "Attempt to open voice pipe for non present modem: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    if (dev->voice_fds[modem_id][FD_CLIENT] != -1) {
        ast_log(AST_LOG_DEBUG, "voice pipe already open: ggw8=%p, modem_id=%d", dev, modem_id);
        return dev->voice_fds[modem_id][FD_CLIENT];
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, dev->voice_fds[modem_id])) {
        ast_log(AST_LOG_ERROR, "Failed to open voice pipe: ggw8=%p, modem_id=%d, errno=%d\n", dev, modem_id, errno);
        return -1;
    }

    //set file descriptors non-blocking
    fd_set_blocking(dev->voice_fds[modem_id][FD_CLIENT], 0);
    fd_set_blocking(dev->voice_fds[modem_id][FD_SRV], 0);

    //enable device to send/receive voice data
    ggw8_voice_ctl(dev, modem_id, format, jbsize);

    ast_log(AST_LOG_DEBUG, "Voice pipe open: ggw8=%p, modem_id=%d, voice_fd=%d, format=%u, jbsize=%u\n",
            dev, modem_id, dev->voice_fds[modem_id][FD_CLIENT], format, jbsize);

    return dev->voice_fds[modem_id][FD_CLIENT];
}

int ggw8_close_voice(struct ggw8_device *dev, uint8_t modem_id)
{
    //ast_log(AST_LOG_DEBUG, "Voice pipe closing: ggw8=%p, modem_id=%d\n", dev, modem_id);

    if (modem_id >= MAX_MODEMS_ON_BOARD) {
        ast_log(AST_LOG_ERROR, "Invalid modem_id: ggw8=%p, modem_id=%d", dev, modem_id);
        return -1;
    }

    int ret = 0;

    if (dev->voice_fds[modem_id][FD_SRV] != -1) {
        ret = close(dev->voice_fds[modem_id][FD_SRV]);
        dev->voice_fds[modem_id][FD_SRV] = -1;
        ret = 1;
    }

    if (dev->voice_fds[modem_id][FD_CLIENT] != -1) {
        ret |= close(dev->voice_fds[modem_id][FD_CLIENT]);
        dev->voice_fds[modem_id][FD_CLIENT] = -1;
        ret = 1;
    }

    if (ret) {
        //disable device to send/receive voice data
        ggw8_voice_ctl(dev, modem_id, 0, 0);
        ast_log(AST_LOG_DEBUG, "Voice pipe close: ggw8=%p, modem_id=%d\n", dev, modem_id);
    }
    return 0;
}

int ggw8_dfu_request(struct ggw8_device *dev,
                     unsigned char *req_buf,
                     int req_buf_len,
                     unsigned char *resp_buf,
                     int resp_buf_len,
                     int *resp_bytes_received)
{
    int rc;
    uint8_t buf[256];
    int attempts = INTERRUPT_IN_DFU_TIMEOUT_MS/1000;
    int bytes_sent;

    rc = interrupt_out(dev, AT_OUT_EP,  req_buf, req_buf_len, &bytes_sent);
    if (rc == 0) {
        DEBUG_HAL("Sent DFU request to device: cmd=0x%.2x, mcu=0x%.2x, data_len=%d\n", req_buf[0], req_buf[1], req_buf_len-2);
    } else {
        ERROR_HAL("Error sending DFU request: rc=%d\n", rc);
        goto exit_;
    }

    while (1) {
        *resp_bytes_received = 0;
        //use the same buffer for getting response
        rc = interrupt_in(dev, AT_IN_EP, buf, (int)sizeof(buf), resp_bytes_received, 1000);
        if (rc == LIBUSB_ERROR_TIMEOUT) {
            if (--attempts < 0) {
                ERROR_HAL("DFU event timedout: timeout=%d\n", INTERRUPT_IN_DFU_TIMEOUT_MS);
                goto exit_;
            }
            continue;
        } else if (rc != 0) {
            ERROR_HAL("Error receiving DFU event: rc=%d\n", rc);
            goto exit_;
        }

        //dfu response header equals to 3 bytes
        if (*resp_bytes_received < resp_buf_len) {
            ERROR_HAL("DFU response to small: dfu_resp_bytes_received=%d, expected=%d\n", *resp_bytes_received, resp_buf_len);
            rc = 1;
            goto exit_;
        }
        /*
        else if (*resp_bytes_received > resp_buf_len) {
            ERROR_HAL("DFU resp_buf to small: dfu_resp_buf_len=%d, dfu_resp_bytes_received=%d\n", resp_buf_len, *resp_bytes_received);
            rc = 1;
            goto exit_;
        }
        */

        //copy response to the caller provided buffer
        //memcpy(resp_buf, buf, *resp_bytes_received);
        memcpy(resp_buf, buf, resp_buf_len);

        if (*resp_buf != *req_buf || *(resp_buf+1) != *(req_buf+1)) {
            DEBUG_HAL("DFU response header mismatch with request header, skip response\n");
            //rc = 1;
            //goto exit_;
            continue;
        }

        DEBUG_HAL("Received DFU response from device: cmd=0x%.2x, mcu=0x%.2x, status=0x%.2x, data_bytes_count=%d\n",
              resp_buf[0], resp_buf[1], resp_buf[2], *resp_bytes_received-3);

        if (resp_buf[2] == DFU_STAT_INPROGRESS) continue;

        //operation complete succsesfully, rc should be equal 0
        break;
    }
exit_:
    return rc;
}

