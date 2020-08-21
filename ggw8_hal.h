/*! \file
 * \author sogurtsov
 * \brief GGW8 device hardware abstraction layer definitions
 */

#ifndef GGW8_HAL_H_INCLUDED
#define GGW8_HAL_H_INCLUDED

#include <asterisk.h>
#include <asterisk/config.h>
#include "rgsm_defs.h"
#include "atomics.h"

#define INVALID_MODEM_ID                    ((int8_t)-1)
#define MAX_MODEMS_ON_BOARD                 8

//! GGW8 error codes
#define GGW8_ERROR_SUCCESS                  0
#define GGW8_ERROR_INVALID_CONTEXT          1
#define GGW8_ERROR_CONTEXT_ALREADY_OPEN     2
#define GGW8_ERROR_TRANSFER_FAILURE         3
#define GGW8_ERROR_NOMEMORY                 4
#define GGW8_ERROR_NODATA                   5
#define GGW8_ERROR_ALREADY_OPEN             6
#define GGW8_ERROR_NODEVICES                7


//! \brief The constants for known voice codecs
#define VC_G701A    0x0001
#define VC_G701U    0x0002
#define VC_G726     0x0004
#define VC_G729A    0x0008
#define VC_GSM      0x0010
#define VC_ILBC     0x0020
#define VC_SPEEX    0x0040

#define GGW8_SERIAL_NUMBER_SIZE     20
#define GGW8_FW_VERSION_SIZE        16
#define GGW8_HW_VERSION_SIZE        16

//! models of GSM modules
#define GGW8_MODULE_NONE            0
#define GGW8_MODULE_SIM900          1

#define GGW8_AT_HEADER_SIZE         3
//#define GGW8_ATOUT_BUFSIZE          ((unsigned short)63)
#define GGW8_ATOUT_BUFSIZE          ((unsigned short)127)
#define GGW8_ATOUT_PAYLOADSIZE      (GGW8_ATOUT_BUFSIZE - GGW8_AT_HEADER_SIZE)

typedef uint16_t ggw8_device_sysid_t;

/**
 * Structure representing a GGW8 device. This is an opaque type for
 * which you are only ever provided with a pointer, usually originating from
 * ggw8_open().
 *
 * A device handle is used to perform I/O and other operations. When finished
 * with a device handle, you should call ggw8_close().
 */
struct ggw8_device;



//! \brief GGW8 device info structure
typedef struct {
    char        serial_number[GGW8_SERIAL_NUMBER_SIZE];
    char        fw_version[GGW8_FW_VERSION_SIZE];
    char        hw_version[GGW8_HW_VERSION_SIZE];
    uint8_t     gsm_presence;                            // each gsm module occupies single bit, 1 - present, 0 - lack
    uint16_t    supported_codecs;
}
__attribute__ ((__packed__)) ggw8_info_t;

//! should return 0 on success, -1 on failure
typedef int ggw8_add_device_callback_t(hw_config_t *cfg, struct ggw8_device *dev);

//! should return true(1) if device with given id is new (may be added) and false(0) otherwise
typedef int ggw8_isnew_device_callback_t(ggw8_device_sysid_t sys_id);

//! \brief GGW8 device control codes
#define GGW8_CTL_RESET              0x01
#define GGW8_CTL_SOFTRESET          0x02
#define GGW8_CTL_DFU                0x03


//! \brief GGW8 device status codes
#define GGW8_STATUS_NONOPERABLE     0x00
#define GGW8_STATUS_OPERABLE        0x01
#define GGW8_STATUS_DFUOPERABLE     0x02

//! \brief GGW8 device modem control codes
#define GGW8_CTL_MODEM_POWERON      0x01
#define GGW8_CTL_MODEM_POWEROFF     0x02
#define GGW8_CTL_MODEM_RESET        0x03
#define GGW8_CTL_MODEM_BOOT         0x04

#define GGW8_MAX_VOICE_DATA_SIZE    160
typedef struct ggw8_voice_frame {
    uint8_t     modem_id;
    uint16_t    voice_format;
    uint8_t     data_length;
    uint8_t     data[GGW8_MAX_VOICE_DATA_SIZE];
}
ggw8_voice_frame_t;

typedef enum ggw8_mode {
    NON_OPERABLE = 0,
    GSM = 1,
    DFU = 2,
} ggw8_mode_t;

extern atomic_t    is_dfu;

//functions declaration

/*!
 * \brief
 * Initialize the context, must be called before call other HAL functions
 * \return 0 on success or non zero return code on error
 */
int ggw8_init_context();

/*!
 * \brief
 * Close a context
 */
void ggw8_close_context();

/*!
 * \brief
 * Discover GGW8 devices with given attributes on previosly open context
 * \param vid the VID of GGW8 device we interesting for, mandatory
 * \param pid the PID of GGW8 device we interesting for, mandatory
 * \param product_string the PRODUCT_STRING descriptor of GGW8 device we interesting for, may be null
 * \param cb_isnew a callback to check device
 * \param cb_add a callback to pass discovered ggw8_device to the caller
 * \return -1 on error otherwise the number of devices discovered
 */
int ggw8_discover_devices(hw_config_t *cfg, ggw8_isnew_device_callback_t *cb_isnew,
                          ggw8_add_device_callback_t *cb_add);

/*! \brief
 * Closes device and deallocates memory occupied by device structure
 * \param dev the pointer to device structure to close
 */
void ggw8_close_device(struct ggw8_device *dev);

void ggw8_restart_device(struct ggw8_device *dev);

/*!
 * \brief
 * Queries GGW8 device status, \see GGW8_STATUS_XXX constants
 * \param status the
 * \return 0 on success or non zero return code on error, negative error codes are related to libusb
 */
int ggw8_get_device_status(struct ggw8_device *dev, uint8_t *status);

ggw8_mode_t ggw8_get_device_mode(struct ggw8_device *dev);

/*!
 * \brief
 * Queries GGW8 device info
 */
ggw8_info_t *ggw8_get_device_info(struct ggw8_device *dev);

uint16_t ggw8_get_device_sysid(struct ggw8_device *dev);

int ggw8_is_alive(struct ggw8_device *dev);
int ggw8_is_restart(struct ggw8_device *dev);

/*!
 * \brief
 * Send a control code to GGW8 device
 * \param ctl_code the control code, \see GGW8_CTL_XXX constants
 * \return 0 on success or non zero return code on error, negative error codes are related to libusb
 */
int ggw8_device_ctl(struct ggw8_device *dev, uint8_t ctl_code);

/*!
 * \brief
 * Send a control code to GSM modem
 * \param modem_id the id of modem to send ctl code to
 * \param ctl_code the control code, \see GGW8_CTL_MODEM_XXX constants
 * \return 0 on success or non zero return code on error, negative error codes are related to libusb
 *
 * \note The modems on GGW8 board are identified by their index between 0 to 7
 */
int ggw8_modem_ctl(struct ggw8_device *dev, uint8_t modem_id, uint8_t ctl_code);

int ggw8_voice_ctl(struct ggw8_device *dev, uint8_t modem_id, uint16_t format, uint16_t jbsize);

int ggw8_baudrate_ctl(struct ggw8_device *dev, uint8_t modem_id, ggw8_baudrate_t rate);

//! AT interface

/*!
 * \brief
 * Grabs the file descriptor to exchange AT data
 * \param device the device pointer to open channel for
 * \param modem_id the id of modem to open channel for
 * \return open fd or -1
 */
//int ggw8_get_at_fd(struct ggw8_device *dev, uint8_t modem_id);

/*!
 * \brief
 * Opens data channel to read/write the AT commands/responses to/from modem
 * \param device the device pointer to open channel for
 * \param modem_id the id of modem to open channel for
 * \return Upon successful completion, this function shall valid fd; otherwise, -1 shall be returned and errno set to indicate the error.
 */
int ggw8_open_at(struct ggw8_device *dev, uint8_t modem_id);

/*!
 * \brief
 * Closes data channel for given modem
 * \param device the device pointer to close channel for
 * \param modem_id the id of modem to close channel for
 * \return The function returns 0 if successful, -1 to indicate an error, with errno set appropriately
 */
int ggw8_close_at(struct ggw8_device *dev, uint8_t modem_id);

//! VOICE interface

/*!
 * \brief
 * Grabs the file descriptor to exchange voice data
 * \param device the device pointer to open channel for
 * \param modem_id the id of modem to open channel for
 * \return open or -1
 */
//int ggw8_get_voice_fd(struct ggw8_device *dev, uint8_t modem_id);

/*!
 * \brief
 * Opens data channel to read/write the voice data to/from modem
 * \param device the device pointer to open channel for
 * \param modem_id the id of modem to open channel for
 * \param format of voice frames, shopuld be consistent with AST_FORMAT_XXX constants
 * \return Upon successful completion, this function shall return valid fd; otherwise, -1 shall be returned and errno set to indicate the error.
 */
int ggw8_open_voice(struct ggw8_device *dev, uint8_t modem_id, uint16_t format, uint16_t jbsize);

/*!
 * \brief
 * Closes voice data channel for given modem
 * \param device the device pointer to close channel for
 * \param modem_id the id of modem to close channel for
 * \return The function returns 0 if successful, -1 to indicate an error, with errno set appropriately
 */
int ggw8_close_voice(struct ggw8_device *dev, uint8_t modem_id);

/**
 * Sends a DFU request to device and returns DFU response back to caller
 * @param req_buf - a request buffer
 * @param req_buf_len - the length of request buffer
 * @param resp_buf - the caller allocated buffer to store a response to
 * @param resp_buf_len - the length of response buffer
 * @param resp_bytes_received - the pionter to return actual number of bytes received in response
 * @return 0 in success or negative LIBUSB_ERROR_XXX or 1 in case of malformed response
 */
int ggw8_dfu_request(struct ggw8_device *dev,
                     unsigned char *req_buf,
                     int req_buf_len,
                     unsigned char *resp_buf,
                     int resp_buf_len,
                     int *resp_bytes_received);

#endif // GGW8_HAL_H_INCLUDED
