// fwsend.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "libusb.h"

// Firmware dump file
#include "../../firmware_dump.h"

int main()
{
	// Now let's find the USB camera and try to comunicate with it
	// Follow the guide in to install the camera as a WinUSB device: https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-installation
	libusb_context* usbContext;
	if (libusb_init(&usbContext))
	{
		fprintf(stderr, "Failed to init LIBUSB\n");
		return -3;
	}

	// TODO: We only support one camera per system with the function bellow.

	// discover devices
	libusb_device** list;
	libusb_device* found = NULL;
	ssize_t cnt = libusb_get_device_list(usbContext, &list);
	int err = 0;
	if (cnt < 0)
	{
		fprintf(stderr, "Failed to list USB devices\n");
	}
	else
	{

		for (int i = 0; i < cnt; i++) {
			libusb_device* device = list[i];
			struct libusb_device_descriptor descriptor;
			int status = libusb_get_device_descriptor(device, &descriptor);
			if ((status >= 0) && (descriptor.idVendor == 0x0471) && (descriptor.idProduct == 0x2127)) {
				// found
				fprintf(stderr, "Found camera (%d)\n", i);
				libusb_device_handle* devHandle;
				if ((status = libusb_open(device, &devHandle)) != 0)
				{
					fprintf(stderr, "Faile to open camera %d: %d %s\n", i, status, libusb_error_name(status));
				}
				else
				{
					// opened
					// We have a handle. Send the requests....
					int setupPacketsCount = sizeof(firmware) / sizeof(firmware[0]);
					for (int index = 0; index < setupPacketsCount; index++)
					{
						int res = libusb_control_transfer(
							devHandle,
							firmware[index].bmRequestType,
							firmware[index].bRequest,
							firmware[index].wValue,
							firmware[index].wIndex,
							firmware[index].data,
							firmware[index].wLength,
							5 * 1000	// 5 seconds should be enough
						);
						if (res < 0)
						{
							fprintf(stderr, "Control transfer %d failed: %d %s\n", index, res, libusb_error_name(res));
						}
					}
					libusb_close(devHandle);
				}
			}
		}
	}

	libusb_free_device_list(list, 1);
	libusb_exit(usbContext);
}
