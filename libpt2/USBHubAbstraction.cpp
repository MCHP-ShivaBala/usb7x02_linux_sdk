/*
**********************************************************************************

©  [2018] Microchip Technology Inc. and its subsidiaries.
Subject to your compliance with these terms, you may use Microchip software and
any derivatives exclusively with Microchip products. It is your responsibility
to comply with third party license terms applicable to your use of third party
software (including open source software) that may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER EXPRESS,
IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES
OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE. IN
NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN
ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST
EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY, THAT YOU
HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

**********************************************************************************
* $File:  USBHubAbstraction.cpp
* Description : File holds API for used for file handling and data transfer
**********************************************************************************/

#include <stdio.h>
#include <libusb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include "MchpUSBInterface.h"
#include "USBHubAbstraction.h"

FILE *fp;

int32_t ReadBinfile(char const *name, uint8_t *buffer)
{
	FILE *file;
	int32_t fileLen;

	//Open file
	file = fopen(name, "rb");
	if (!file)
	{
		fprintf(stderr, "Unable to open file %s\n", name);
		return 0;
	}

	//Get file length
	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	fseek(file, 0, SEEK_SET);

	//Read file contents into buffer
	fread(buffer, fileLen, 1, file);
	fclose(file);
	return fileLen;
}

uint32_t writeBinfile(char const *name, uint8_t *buffer, unsigned long fileLen)
{
	FILE *file;
    uint32_t CharWritten = -1;

	//Open file
	file = fopen(name, "wb+");
	if (!file)
	{
		fprintf(stderr, "Unable to open file %s", name);
		return 0;
	}

	//Read file contents into buffer
	CharWritten = fwrite(buffer, 1, fileLen, file);
	fclose(file);
	return CharWritten;
}

int  usb_HCE_read_data(PUSB_CTL_PKT pUsbCtlPkt)
{
	int rc = 0;

	rc = libusb_control_transfer(	pUsbCtlPkt->handle,
									LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
									pUsbCtlPkt->byRequest,
									pUsbCtlPkt->wValue,
									pUsbCtlPkt->wIndex,
									pUsbCtlPkt->byBuffer,
									pUsbCtlPkt->wLength,
									CTRL_TIMEOUT
								);

	return rc;
}

int  usb_HCE_write_data(PUSB_CTL_PKT pUsbCtlPkt)
{
	int rc = 0;

	rc = libusb_control_transfer(	pUsbCtlPkt->handle,
									LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
									pUsbCtlPkt->byRequest,
									pUsbCtlPkt->wValue,
									pUsbCtlPkt->wIndex,
									pUsbCtlPkt->byBuffer,
									pUsbCtlPkt->wLength,
									CTRL_TIMEOUT
								);
	return rc;
}
int  usb_HCE_no_data(PUSB_CTL_PKT pUsbCtlPkt)
{
	int rc = 0;

	rc = libusb_control_transfer(	pUsbCtlPkt->handle,
									LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
									pUsbCtlPkt->byRequest,
									pUsbCtlPkt->wValue,
									pUsbCtlPkt->wIndex,
									NULL,
									0,
									CTRL_TIMEOUT
								);

	return rc;
}
