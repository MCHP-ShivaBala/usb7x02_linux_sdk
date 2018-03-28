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
*   spi_bridging.cpp
*   This file gives the sample code/ test code for using Mchp USB70xx API
*	Interface.
**********************************************************************************
*  $Revision:
*  Description: Sample code for SPI Bridging/Programming
*  Author: Shiva Balasubramanian <shiva.balasubramanian@microchip.com>
**********************************************************************************
* $File:
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <stdbool.h>

#include <cstdlib>

// MPLABConnect SDK API Header file
#include "MchpUSBInterface.h"
#include "USBHubAbstraction.h"

using namespace std;

int main (int argc, char* argv[])
{
	char sztext[1024];

	DWORD dwError = 0;
	uint16_t vendor_id = 0x424 ,product_id= 0x7002;
	uint8_t byOperation;
    uint32_t byStartAddr = 0;
	uint8_t hDevice =  INVALID_HANDLE_VALUE;
    uint8_t bySpiRomBootflag, byReboot = 0;

    uint8_t  pbyBuffer[256 * 1024]; //SB
	int32_t wDataLength;
    char *sFirmwareFile;

	// uint8_t byBuffer[5] = {0,0,0,0}; //SB
    uint8_t byJedecIDBuffer[4] = {0,0,0,0};
	// uint16_t DataLen, wTotalLen;

    char path[20] = {0};
	uint8_t hub_count = 0;

	if((0 == strcmp(argv[1],"--help")) || (0 == strcmp(argv[1],"/?")))  //Help
	{
		printf("Operation : Write \n");
		printf("Usage: ./spiBridging VID(Hex) PID(Hex) DevicePath(String) Operation(0x01) FirmwareFile \n");
		printf("Example: ./spiBridging 0x424 0x4002 \"1:2\" 0x01 USB7x02_SILICON_SPI_V1.27.bin \n \n");
		printf("Operation : Read \n");
		printf("Usage: ./spiBridging VID(Hex) PID(Hex) DevicePath(String) Operation(0x00) FirmwareFile \n");
		printf("Example: ./spiBridging 0x424 0x4002 \"1:2\" 0x00 flash_dump.bin \n\n");
		// printf("Operation : Transfer\n");
		// printf("Usage: ./spiBridging VID(Hex) PID(Hex) DevicePath(String) Operation(0x03) Command DataLength TotalLength\n");
		// printf("Example: ./spiBridging 0x0424 0x4002 8 0x03 0x9f 1 4 \n\n");
		exit(1);

	}
	else if(argc < 6)
	{
		printf("ERROR: Invalid Usage.\n");
		printf("Use --help option for further details \n");
		exit (1);
	}
	else
	{
		vendor_id  =  strtol (argv[1], NULL, 0) ;          //Getting vid and Pid from commandline arguments.
		product_id =  strtol (argv[2], NULL, 0) ;
		strcpy(path,argv[3]);
		byOperation=  strtol (argv[4], NULL, 0) ;

		if(byOperation == 0x00) // Read.
		{
            sFirmwareFile = argv[5];
            byStartAddr = 0x00;
		}
		else if(byOperation == 0x01) //Write
		{
			sFirmwareFile = argv[5];
			byStartAddr = 0x00;				//SB
		}
		else if(byOperation == 0x03) //Transfer
		{
			// byBuffer[0] = strtol (argv[5],NULL,0);	//SB
			// DataLen	 = strtol (argv[6],NULL,0);
			// wTotalLen = strtol (argv[7],NULL,0);
            //
            // byStartAddr = strtol (argv[8], NULL, 0);    //SB
            // byBuffer[1] = (byStartAddr & 0xFF0000) >> 16; //SB
            // byBuffer[2] = (byStartAddr & 0x00FF00) >> 8; //SB
            // byBuffer[3] = byStartAddr & 0x0000FF; //SB
		}
	}

	// // Get the version number of the SDK
	// if (FALSE == MchpUsbGetVersion(sztext))
	// {
	// 	printf ("\nError:SDK Version cannot be obtained,Press any key to exit....");
	// 	exit (1);
	// }

    printf("\n***** MPLABConnect Linux SDK for USB7x02 *****\n");
    printf("SPI Programming Example\n\n");

	memset(sztext,0,2048);

	hub_count = MchpGetHubList(sztext);

	if(hub_count < 0)
	{
		printf("No USB hubs found in the system");

	}

	//Return handle to the first instance of VendorID and ProductID matched device.
    printf ("Opening the hub... \n\n");
    hDevice = MchpUsbOpen(vendor_id,product_id,path);
	if(INVALID_HANDLE_VALUE == hDevice)
	{
		printf ("\nError: MchpUsbOpenID Failed:\n");
		exit (1);
	}
    DEBUGPRINT("MchpUsbOpenID successful... \n\n");

    do
    {
        //find whether device boots from SPI ROM or Int ROM
        printf("Gathering Hub Information...\n");
        get_hub_info(hDevice, (uint8_t *)&gasHubInfo[hDevice].sHubInfo);

        printf("Hub Silicon Revision: %02x\n", gasHubInfo[hDevice].sHubInfo.byDeviceRevision);
        printf("Hub Firmware Revision: %02x\n", gasHubInfo[hDevice].sHubInfo.wInternalFWRevision);

        if(gasHubInfo[hDevice].sHubInfo.byFeaturesFlag & 0x01)
        {
            bySpiRomBootflag = TRUE;
            printf ("Hub executing from SPI ROM...Forcing Hub to boot from Int ROM... \n\n");

            //Force Booting from Internal ROM
            ForceBootFromRom(hDevice);

            //Releasing the existing device handle
            if(FALSE == MchpUsbClose(hDevice))
            {
                dwError = MchpUsbGetLastErr(hDevice);
                printf ("\nMchpUsbClose:Error Code,%04x\n",(unsigned int)dwError);
                exit (1);
            }

            printf ("Re-opening the hub... \n\n");
            //Re-opening the hub to get a new device handle
            hDevice = MchpUsbOpen(vendor_id,product_id,path);
            if(INVALID_HANDLE_VALUE == hDevice)
            {
                printf ("\nError: MchpUsbOpenID Failed:\n");
                exit (1);
            }
            DEBUGPRINT("MchpUsbOpenID successful... \n\n");

        }
        else
        {
            bySpiRomBootflag = FALSE;
            printf ("Hub executing from Int ROM... \n");
        }
    } while(bySpiRomBootflag || ((++byReboot)>3));

    if(bySpiRomBootflag)
    {
        printf ("Failed to force boot from ROM... \n");
        exit(1);
    }

    if(FALSE == GetJEDECID(hDevice, byJedecIDBuffer))
    {
        printf ("Failed to read the SPI Flash Manufacturer ID:\n");
        exit (1);
    }

    if(byJedecIDBuffer[0] != MICROCHIP_SST_FLASH)
    {
        printf("Warning: Non-Microchip Flash are not supported. Operation might fail or have unexpected results\n");
        printf("Do you wish to continue (Choose y or n):");
        if(getchar() == 'n')
        {
            printf("Exiting...\n");
            exit(1);
        }
        else
        {
            printf("\n");
        }
    }

	if(byOperation == 0x00) //Read
	{
        //Performs read operation from SPI Flash.
		if(FALSE == MchpUsbSpiFlashRead(hDevice,byStartAddr,pbyBuffer,MAX_FW_SIZE))
		{
			printf ("\nError: Read Failed:\n");
			exit (1);
		}

        if(writeBinfile(sFirmwareFile, pbyBuffer, MAX_FW_SIZE) < 0)
        {
            printf("Error: Failed to create binary file\n");
        }
	}
	else if(byOperation == 0x01)//Write
	{
		//Read the content of the file.
		wDataLength = ReadBinfile(sFirmwareFile,pbyBuffer);
		if(wDataLength <=0)
		{
			printf("Failed to Read Content of File\n");
			exit (1);
		}
		//Performs write opeartion to SPI Flash memory.
		if(FALSE == MchpUsbSpiFlashWrite(hDevice,byStartAddr, pbyBuffer,wDataLength))
		{
			printf ("\nError: Write Failed:\n");
			exit (1);
		}

        //Re-opening the hub to get a new device handle
        hDevice = MchpUsbOpen(vendor_id,product_id,path);
        if(INVALID_HANDLE_VALUE == hDevice)
        {
            printf ("\nError: MchpUsbOpenID Failed:\n");
            exit (1);
        }

        //Checking Firmware version post programming
        printf("Gathering Hub Information...\n");
        get_hub_info(hDevice, (uint8_t *)&gasHubInfo[hDevice].sHubInfo);

        printf("Hub Silicon Revision: %02x\n", gasHubInfo[hDevice].sHubInfo.byDeviceRevision);
        printf("Hub Firmware Revision: %02x\n", gasHubInfo[hDevice].sHubInfo.wInternalFWRevision);

    }

	//close device handle
	if(FALSE == MchpUsbClose(hDevice))
	{
		dwError = MchpUsbGetLastErr(hDevice);
		printf ("\nMchpUsbClose:Error Code,%04x\n",(unsigned int)dwError);
		exit (1);
	}
	return 0;
}
