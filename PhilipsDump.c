#define _GNU_SOURCE
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "firmware.h"

#define STR_BEGINS_WITH(str, prefix) strncmp(str, prefix, strlen(prefix))
#define MAX_LINE_LEN (512)

/* run this program using the console pauser or add your own getch, system("pause") or input loop */
struct SETUP_pck setupPackets[MAX_SETUP_PACKET];
static int setupPacketsCount = 0;

enum eSTATE
{
	IDLE,
	SETUP_PCK_HEADER,
	SETUP_PCK_DATA_D2H,
	SETUP_PCK_DATA_H2D
};

int main(int argc, char* argv[]) {
	char			*filename;
	char			line[MAX_LINE_LEN];
	size_t			lineSz;
	FILE			*file;
	enum eSTATE		state = IDLE;
	int				rasc;

	// statistcs
	uint32_t linesRead = 0;

	if (argc < 2) return -1;

	filename = argv[1];
	if (fopen_s(&file, filename, "r"))
	{
		return -2;
	}

	rewind(file);
	while(fgets(line, MAX_LINE_LEN, file))
	{
		linesRead++;
		lineSz = strlen(line);
		while ((lineSz > 0) && ((line[lineSz - 1] == '\r') || (line[lineSz - 1] == '\n')))
		{
			// remove \\r \\n
			line[lineSz - 1] = '\0';
			lineSz--;
		}
		//printf("line %8d:\t%s\r\n", linesRead, line);


		switch (state)
		{
		case IDLE:
			if (!strcmp(line, "SETUP address: <MAXIM> <timestamp>"))
			{
				// Found new start of package
				memset(&setupPackets[setupPacketsCount], 0x00, sizeof(struct SETUP_pck));
				state = SETUP_PCK_HEADER;
			}
			else
			{
				fprintf(stderr, "UNKNOWN LINE AT IDLE STATE! (%d)\n", linesRead);
			}
			break; // case IDLE:

		case SETUP_PCK_HEADER:
			// read each member
			if (!STR_BEGINS_WITH(line, "\tbmRequestType="))
			{
				sscanf(line + 15, "%x", &rasc);
				if ((uint8_t)(rasc & 0b01100000) != 0b01000000)
					state = IDLE; // We are only interested in Vendor Requests

				setupPackets[setupPacketsCount].bmRequestType = (uint8_t)(rasc & 0xFF);
			}

			else if (!STR_BEGINS_WITH(line, "\tbRequest="))
			{
				sscanf(line + 10, "%x", &rasc);
				setupPackets[setupPacketsCount].bRequest = (uint8_t)(rasc & 0xFF);
			}

			else if (!STR_BEGINS_WITH(line, "\twValue="))
			{
				sscanf(line + 8, "%x", &rasc);
				setupPackets[setupPacketsCount].wValue = (uint16_t)(rasc & 0xFFFF);
			}

			else if (!STR_BEGINS_WITH(line, "\twIndex="))
			{
				sscanf(line + 8, "%x", &rasc);
				setupPackets[setupPacketsCount].wIndex = (uint16_t)(rasc & 0xFFFF);
			}

			else if (!STR_BEGINS_WITH(line, "\twLength="))
			{
				sscanf(line + 9, "%x", &rasc);
				setupPackets[setupPacketsCount].wLength = (uint16_t)(rasc & 0xFFFF);
			}

			else if ((!strcmp(line, "DATA <timestamp>")) || (!strcmp(line, "STATUS <timestamp>")))
			{
				// Start of DATA section
				if(setupPackets[setupPacketsCount].bmRequestType & 0x80) state = SETUP_PCK_DATA_D2H;
				else state = SETUP_PCK_DATA_H2D;
			}

			else
			{
				fprintf(stderr, "UNKNOWN LINE AT SETUP_PCK_HEADER STATE! (%d)\n", linesRead);
			}

			break; // case SETUP_PCK_HEADER:

		case SETUP_PCK_DATA_D2H:
			if (!strcmp(line, "\t<data IN packet NAKed by device. <timestamp>>"))
			{
				// just ignore line
				continue;
			}

			else if (!STR_BEGINS_WITH(line, "\tbyte="))
			{
				sscanf(line + 6, "%x", &rasc);
				setupPackets[setupPacketsCount].data[setupPackets[setupPacketsCount].dataActualLen++] = (uint8_t)(rasc & 0xFF);
			}

			else if (!strcmp(line, "STATUS <timestamp>"))
			{
				// Ignore line
				continue;
			}

			else if (!strcmp(line, "\tACK"))
			{
				// End of packet. Go back to IDLE
				if(setupPackets[setupPacketsCount].dataActualLen != setupPackets[setupPacketsCount].wLength)
					fprintf(stderr, "PACKET WRITE SIZE NOT EQUAL LENGTH! (%d)\n", linesRead);
				setupPacketsCount++;
				state = IDLE;
			}

			else if (!strcmp(line, "\tSTALL"))
			{
				// Stalled packet
				fprintf(stderr, "PACKET STALLED! (%d)\n", linesRead);

				setupPacketsCount++;
				state = IDLE;
			}

			else
			{
				fprintf(stderr, "UNKNOWN LINE AT SETUP_PCK_DATA_D2H STATE! (%d)\n", linesRead);
			}

			break; // case SETUP_PCK_DATA_D2H

		case SETUP_PCK_DATA_H2D:
			if (!strcmp(line, "\t<data OUT packet NAKed by device. <timestamp>>"))
			{
				// Packet will be sent again!
				if (setupPackets[setupPacketsCount].dataActualLen < 64)
				{
					fprintf(stderr, "PACKED NAKed ON LESS THAN 64B received! (%d)\n", linesRead);
				}
				else
				{
					if (setupPackets[setupPacketsCount].dataActualLen % 64)
					{
						fprintf(stderr, "PACKET NAKed NOT MULTIPLE OF 64B! (%d)\n", linesRead);
						setupPackets[setupPacketsCount].dataActualLen -= setupPackets[setupPacketsCount].dataActualLen % 64;
					}
					else
					{
						setupPackets[setupPacketsCount].dataActualLen -= 64;
					}
				}
			}

			else if (!STR_BEGINS_WITH(line, "\tbyte="))
			{
				sscanf(line + 6, "%x", &rasc);
				setupPackets[setupPacketsCount].data[setupPackets[setupPacketsCount].dataActualLen++] = (uint8_t)(rasc & 0xFF);
			}

			else if (!strcmp(line, "STATUS <timestamp>"))
			{
				// Ignore line
				continue;
			}

			else if (!strcmp(line, "\tACK"))
			{
				// End of packet. Go back to IDLE
				if (setupPackets[setupPacketsCount].dataActualLen != setupPackets[setupPacketsCount].wLength)
					fprintf(stderr, "PACKET READ SIZE NOT EQUAL LENGTH! (%d)\n", linesRead);
				setupPacketsCount++;
				state = IDLE;
			}

			else
			{
				fprintf(stderr, "UNKNOWN LINE AT SETUP_PCK_DATA_H2D STATE! (%d)\n", linesRead);
			}
			break; // case SETUP_PCK_DATA_H2D
		}
	}
	fclose(file);
	
	
	// Finished reading all packages
	// Dump them in header file
	filename = "firmware_dump.h";
	if (fopen_s(&file, filename, "w"))
	{
		return -2;
	}

	fprintf(file, "#include \"firmware.h\"\n");
	fprintf(file, "static struct SETUP_pck firmware[%d] = {\n", setupPacketsCount);
	for (int index = 0; index < setupPacketsCount; index++)
	{
		fprintf(file, "/* Packet %3d */  {", index);
		fprintf(file, ".bmRequestType = 0x%02x, ", setupPackets[index].bmRequestType);
		fprintf(file, ".bRequest = 0x%02x, ", setupPackets[index].bRequest);
		fprintf(file, ".wIndex = 0x%04x, ", setupPackets[index].wIndex);
		fprintf(file, ".wLength = 0x%04x, ", setupPackets[index].wLength);
		fprintf(file, ".wValue = 0x%04x, ", setupPackets[index].wValue);
		fprintf(file, ".dataActualLen = 0x%04x, ", setupPackets[index].dataActualLen);
		fprintf(file, ".data = {");
		if (setupPackets[index].dataActualLen)
			for (int jndex = 0; jndex < setupPackets[index].dataActualLen; jndex++) fprintf(file, "0x%02x%s", setupPackets[index].data[jndex], jndex == setupPackets[index].dataActualLen - 1 ? "" : ", ");
		else
			fprintf(file, "0x00");
		fprintf(file, "}");
		fprintf(file, "}%s /* Packet %3d */\n", index == setupPacketsCount-1?"":",", index);
	}
	fprintf(file, "};\n");
	fclose(file);
	return 0;
}