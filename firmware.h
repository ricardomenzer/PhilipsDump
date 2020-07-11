#pragma once

#define MAX_SETUP_PACKET	(255)
#define MAX_PACKET_DATA_LEN	(0x1000)
struct SETUP_pck
{
	uint8_t		bmRequestType;
	uint8_t 	bRequest;
	uint16_t	wValue;
	uint16_t	wIndex;
	uint16_t	wLength;
	uint8_t		data[MAX_PACKET_DATA_LEN];
	uint16_t	dataActualLen;
};
