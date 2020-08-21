//#include <cassert>
//#include <cstdio>
#include <stdio.h>
#include <libusb.h>

int main() {
    libusb_context *context = NULL;
    libusb_device **list = NULL;
    int rc = 0;
    ssize_t count = 0;

    rc = libusb_init(&context);
//    assert(rc == 0);

    count = libusb_get_device_list(context, &list);
//    assert(count > 0);

    for (size_t idx = 0; idx < count; ++idx) {
        libusb_device *device = list[idx];
        struct libusb_device_descriptor desc = {0};

        rc = libusb_get_device_descriptor(device, &desc);
//        assert(rc == 0);

        printf("Bus %03x Device %03x: ID %04x:%04x\n", libusb_get_bus_number(device), libusb_get_device_address(device), desc.idVendor, desc.idProduct);
    }
	libusb_free_device_list(list, 1);
}
