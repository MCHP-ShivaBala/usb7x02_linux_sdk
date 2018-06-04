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
*  $Revision:
*  Description: This version supports SPI Bridging and SPI Flash Programming
**********************************************************************************
* $File:  MchpUSBINterface.cpp
*/

#include <stdio.h>
#include <libusb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

//MPLABConnect SDK Header file.
#include "typedef.h"
#include "MchpUSBInterface.h"
#include "USBHubAbstraction.h"

//DLL Exports
#define FALSE								0
#define TRUE								1
#define MICROCHIP_HUB_VID					0x424
#define VID_MICROCHIP						0x0424

#define CMD_DEV_RESET                       0x29
#define BOOT_FROM_ROM_ON_RESET              0x08

#define CMD_SPI_PASSTHRU_ENTER				0x60
#define CMD_SPI_PASSTHRU_EXIT				0x62
#define CMD_SPI_PASSTHRU_WRITE              0x61

#define HUB_STATUS_BYTELEN						3 /* max 3 bytes status = hub + 23 ports */
#define HUB_SKUs                                6

#define logprint(x, ...) do { \
		printf(__VA_ARGS__); \
		fprintf(x,  __VA_ARGS__); \

//Microchip SPI Flash Specific Macros
#define WRITE_BLOCK_SIZE                    256
#define READ_BLOCK_SIZE                     512
#define READ_JEDEC_ID					    0x9F
#define	WREN							    0x06 //Write latch enable
#define WRDIS                               0X04 //Write latch disable
#define	RDSR								0x05 //Read Status Reg
#define ULBPR								0x98 //Unblock Global Protection
#define CHIP_ERASE							0xC7
#define BLOCK_ERASE                         0xD8
#define PAGE_PROG							0x02
#define HS_READ                             0x0B
#define RSTQIO                              0xFF //Reset Quad IO Interface

/*-----------------------Helper functions --------------------------*/
int usb_enable_HCE_device(uint8_t hub_index);
static int compare_hubs(const void *p1, const void *p2);
static int usb_get_hubs(PHINFO pHubInfoList);
static int usb_get_hub_list(PCHAR pHubInfoList);
static int usb_open_HCE_device(uint8_t hub_index);
int usb_send_vsm_command(struct libusb_device_handle *handle, uint8_t * byValue) ;
int xdata_read(HANDLE handle, uint32_t wAddress, uint8_t *data, uint8_t num_bytes);
int xdata_write(HANDLE handle, uint32_t wAddress, uint8_t *data, uint8_t num_bytes);

 // Global variable for tracking the list of hubs
HINFO gasHubInfo [MAX_HUBS];

/* Context Variable used for initializing LibUSB session */
libusb_context *ctx = NULL;

/*List of possible PIDs for USB7x02 HFCs*/
uint16_t PID_HCE_DEVICE[HUB_SKUs] = {0x7040, 0x704A, 0x704B, 0x704C, 0x704E, 0x704F};

/*-----------------------API functions --------------------------*/
// Get last error for the specific hub instance.
UINT32 MchpUsbGetLastErr (HANDLE DevID)
{
	DevID = DevID;
	return errno;
}

int MchpGetHubList(PCHAR pchHubcount )
{
	int hub_count =0;
	hub_count  = usb_get_hub_list(pchHubcount);
	return hub_count;
}

//Return handle to the first instance of VendorID &amp; ProductID &amp; port path matched device.
HANDLE  MchpUsbOpen ( UINT16 wVID, UINT16 wPID,char* cDevicePath)
{
    	int error = 0, hub_index=0;
    	int restart_count=5;
    	bool bhub_found = false;

	char *ptr;
	BYTE DeviceID[7];
	int  i=0 ;
	ptr = cDevicePath;


	while(*ptr != '\0')
	{
		if(*ptr != ':')
		{
			DeviceID[i] = *ptr - 48;
			i++;
		}
		ptr++;
	}


	if( 0 == usb_get_hubs(&gasHubInfo[0]))
	{
		DEBUGPRINT("MCHP_Error_libusb_error_no_hub \n");
		return INVALID_HANDLE_VALUE;
	}

    	for(;hub_index < 10;hub_index++)
    	{
		if((gasHubInfo[hub_index].wVID == wVID) && (gasHubInfo[hub_index].wPID == wPID) && (i == gasHubInfo[hub_index].port_max))
		{
			 for (int j=0; j<gasHubInfo[hub_index].port_max; j++)
			{
				if(gasHubInfo[hub_index].port_list[j] != DeviceID[j])
				{
					bhub_found = false;
					break;

				}
				bhub_found = true;

			}
			if(true == bhub_found)
			{
				break;
			}


		}

	}

	if(false == bhub_found)
    	{
     		DEBUGPRINT("MCHP_Error_Device_Not_Found \n");
        	return INVALID_HANDLE_VALUE;
   	}

    	error = usb_open_HCE_device(hub_index);

    	if(error < 0)
    	{
        //enable 5th Endpoint
		error = usb_enable_HCE_device(hub_index);

 	       if(error < 0)
        	{
            		DEBUGPRINT("MCHP_Error_Invalid_Device_Handle: Failed to Enable the device \n");
            		return INVALID_HANDLE_VALUE;
        	}
        	do
       		{
            		sleep(2);
            		error = usb_open_HCE_device(hub_index);
            		if(error == 0)
            		{
                 		return hub_index;
            		}

        	}while(restart_count--);

       		DEBUGPRINT("MCHP_Error_Invalid_Device_Handle: Failed to open the device error:%d\n",error);
        	return INVALID_HANDLE_VALUE;
 	}

   	return hub_index;
}


//Close the device handle.
BOOL MchpUsbClose(HANDLE DevID)
{

	if(gasHubInfo[DevID].handle != NULL)
	{
		libusb_close((libusb_device_handle*)gasHubInfo[DevID].handle);
		gasHubInfo[DevID].dev = NULL;
		gasHubInfo[DevID].handle = NULL;
	}
	else
	{
		printf("unknown hub index%d\n", DevID);
		return false;
	}
	libusb_exit(ctx);
	return true;
}

//Enable/Disable the SPI Passthrough Interface
BOOL MchpUsbSpiSetConfig ( HANDLE DevID, INT EnterExit)
{
	int rc = FALSE;
	BOOL bRet = FALSE;
	BYTE byCmd =0;

	if(EnterExit)
	{
		byCmd = CMD_SPI_PASSTHRU_ENTER;
	}
	else
	{
		byCmd = CMD_SPI_PASSTHRU_EXIT;
	}
	rc = libusb_control_transfer((libusb_device_handle*)gasHubInfo[DevID].handle,0x41,byCmd,0,0,0,0,500);
	if(rc < 0)
	{
		DEBUGPRINT("MchpUsbSpiSetConfig failed\n");
		bRet = FALSE;
	}
	else
	{
		DEBUGPRINT("MchpUsbSpiSetConfig Passed\n");
		bRet = TRUE;
	}
	return bRet;
}

/*Pre-req: Hub needs to be open*/
BOOL MchpUsbSpiFlashRead(HANDLE DevID,UINT32 StartAddr,UINT8* InputData,UINT32 BytesToRead)
{
    uint16_t NumPageReads = 0;
    // uint8_t RemainderBytes = 0;
    uint8_t byReadBuffer[READ_BLOCK_SIZE+5];
    uint8_t byBuffer[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

    //Reset Quad IO
    byBuffer[0] = RSTQIO;
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //Reading the JEDEC ID of the flash
    if(FALSE == GetJEDECID(DevID, byBuffer))
    {
        printf ("Failed to read the SPI Flash Manufacturer ID:\n");
        exit (1);
    }

    //Silicon Rev B0 returns a dummy byte in the beginning while rev A0 doesn't
    //if(byBuffer[0] != MICROCHIP_SST_FLASH)
    if(byBuffer[0] != MICROCHIP_SST_FLASH && byBuffer[1] != MICROCHIP_SST_FLASH)
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

    //Reading the second byte of the 32-bit runtime flag register at BFD2_3408 to check bit 13
    //for enabling Pass through function running from ROM
    if(FALSE == xdata_read(DevID, 0xBFD23409, byBuffer, 1))
    {
        printf("Failed to read run time flag register \n");
        exit (1);
    }

    //Setting Bit 13 in the register contents
    byBuffer[0] |= (1 << 5);


    if(FALSE == xdata_write(DevID, 0xBFD23409, byBuffer, 1))
    {
        printf("Failed to write run time flag register\n");
        exit (1);
    }

    //Enable the SPI interface.
    if(FALSE == MchpUsbSpiSetConfig (DevID,1))
    {
        printf ("\nError: SPI Pass thru enter failed:\n");
        exit (1);
    }

    NumPageReads = BytesToRead / READ_BLOCK_SIZE;
    // RemainderBytes = BytesToRead % READ_BLOCK_SIZE;

    for(uint16_t i=0; i<NumPageReads; i++)
    {
        // //Writing the HS_READ command
        // byBuffer[0] = HS_READ;
        // byBuffer[1] = (StartAddr & 0xFF0000) >> 16;
        // byBuffer[2] = (StartAddr & 0x00FF00) >> 8;
        // byBuffer[3] = StartAddr & 0x0000FF;
        // byBuffer[4] = 0;

        //Creating the Data OUT packet for SETUP for SPI transfer read with auto
        //polling of flash BUSY bit
        byBuffer[0] = 'S'; //Signature
        byBuffer[1] = 'P'; //Signature
        byBuffer[2] = 'I'; //Signature
        byBuffer[3] = 'D'; //Signature
        byBuffer[4] = 0x01; //Busy Bit Mask
        byBuffer[5] = RDSR; //Read Status opcode
        byBuffer[6] = 0x00; //No of dummy bytes
        byBuffer[7] = ((gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) ? 0x01:0x00);//Response buffer index
                                              //for polling BUSY bit. For Silicon rev A0: 0x00 and for rev B0: 0x01

        byBuffer[8] = HS_READ;// High Speed read instruction
        byBuffer[9] = (StartAddr & 0xFF0000) >> 16;
        byBuffer[10] = (StartAddr & 0x00FF00) >> 8;
        byBuffer[11] = StartAddr & 0x0000FF;
        byBuffer[12] = 0x00; //Dummy address byte

        //Sending the SETUP packet for reading a Block
        // if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,5,READ_BLOCK_SIZE+5)) //write
        if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,13,READ_BLOCK_SIZE+5+13)) //write //B0
        {
            printf("SPI Transfer write failed \n");
            exit (1);
        }

        DEBUGPRINT("Reading page %d at addr 0x%06x\n",i,StartAddr);

        //Reading READ_BLOCK_SIZE bytes at a time
        if(FALSE == MchpUsbSpiTransfer(DevID,1,(uint8_t *)&byReadBuffer,READ_BLOCK_SIZE,READ_BLOCK_SIZE+5)) //parameter 4 is don't care
        {
            printf("SPI Transfer read failed \n");
            exit (1);
        }

        /*Copying the READ_BLOCK_SIZE of data read into local buffer for writing to binary file*/

        if(gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) //For silicon rev B0, ignore the first 5 bytes in th response
        {
            memcpy((void *)&InputData[i*READ_BLOCK_SIZE], (const void *)&byReadBuffer[5], READ_BLOCK_SIZE);
        }
        else
        {
            memcpy((void *)&InputData[i*READ_BLOCK_SIZE], (const void *)&byReadBuffer[0], READ_BLOCK_SIZE);
        }


        // //Check if the flash is BUSY
        // byBuffer[0] = RDSR;
        // byBuffer[1] = 0;
        // do
        // {
        //     //performs write operation to the SPI Interface.
        //     if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],2,3)) //write
        //     {
        //         printf("SPI Transfer write failed \n");
        //         exit (1);
        //     }
        //
        //     if(FALSE == MchpUsbSpiTransfer(DevID,1,(uint8_t *)&byReadBuffer,2,1)) //3rd Argument is don't care
        //     {
        //         printf("SPI Transfer read failed \n");
        //         exit (1);
        //     }
        //     // DEBUGPRINT("Reading page %d at addr 0x%06x...SR = %02x\n",i,StartAddr,byReadBuffer[0]);
            // DEBUGPRINT("Reading page %d at addr 0x%06x...SR = %02x\n",i,StartAddr,byReadBuffer[1]);

        //}while(byReadBuffer[0] != 0x00);
        // }while(byReadBuffer[1] & 0x01);

        StartAddr += READ_BLOCK_SIZE;

        //Printing process completion
        if(i == 0)
        {
            printf("\n\nReading SPI Flash...\n\n0%% ");
        }
        if(i%5 == 0)
        {
            printf("#");
        }
        if(i == NumPageReads-1)
        {
            printf(" DONE\n\n");
        }
    }

    //Disable the SPI interface.
    if(FALSE == MchpUsbSpiSetConfig (DevID,0))
    {
        printf ("Error: SPI Pass thru enter failed:\n");
        exit (1);
    }
    return TRUE;
}

/*Pre-req: Hub needs to be open*/
BOOL MchpUsbSpiFlashWrite(HANDLE DevID,UINT32 StartAddr,UINT8* OutputData, UINT32 BytesToWrite)
{
	BOOL bRet = FALSE;
    uint16_t NumPageWrites = 0;
    uint8_t RemainderBytes = 0;
    // uint8_t byWriteBuffer[WRITE_BLOCK_SIZE+4] = {PAGE_PROG};
    uint8_t byWriteBuffer[WRITE_BLOCK_SIZE+12];
    uint8_t byBuffer[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t byVerifyBuffer[MAX_FW_SIZE];

	if(nullptr == OutputData)
	{
		DEBUGPRINT("SPI Write failed: NULL pointer");
		return FALSE;
	}

	if ((StartAddr + BytesToWrite) > MAX_FW_SIZE)
	{
		DEBUGPRINT("MchpUsbSpiFlashWrite Failed: BytesToWrite (%d) and StartAddr(0x%x) is larger than SPI memory size\n",BytesToWrite,StartAddr);
		return bRet;
	}

    //Reset Quad IO
    byBuffer[0] = RSTQIO;
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //Reading the JEDEC ID of the flash
    if(FALSE == GetJEDECID(DevID, byBuffer))
    {
        printf ("Failed to read the SPI Flash Manufacturer ID:\n");
        exit (1);
    }

    //Silicon Rev B0 returns a dummy byte in the beginning while rev A0 doesn't
    //if(byBuffer[0] != MICROCHIP_SST_FLASH)
    if(byBuffer[0] != MICROCHIP_SST_FLASH && byBuffer[1] != MICROCHIP_SST_FLASH)
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

    //Reading the second byte of the 32-bit runtime flag register at BFD2_3408 to check bit 13
    //for enabling Pass through function running from ROM
    if(FALSE == xdata_read(DevID, 0xBFD23409, byBuffer, 1))
    {
        printf("Failed to read run time flag register \n");
        exit (1);
    }

    //Setting Bit 13 in the register contents
    byBuffer[0] |= (1 << 5);


    if(FALSE == xdata_write(DevID, 0xBFD23409, byBuffer, 1))
    {
        printf("Failed to write run time flag register\n");
        exit (1);
    }

    //Enable the SPI interface.
    if(FALSE == MchpUsbSpiSetConfig (DevID,1))
    {
        printf ("\nError: SPI Pass thru enter failed:\n");
        exit (1);
    }

    //WREN
    byBuffer[0] = WREN;
    //performs write operation to the SPI Interface.
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //Unblock Global Protection
    // byBuffer[0] = ULBPR;

    byBuffer[0] = 'S'; //Signature
    byBuffer[1] = 'P'; //Signature
    byBuffer[2] = 'I'; //Signature
    byBuffer[3] = 'D'; //Signature
    byBuffer[4] = 0x01; //Busy Bit Mask
    byBuffer[5] = RDSR; //Read Status opcode
    byBuffer[6] = 0x00; //No of dummy bytes
    byBuffer[7] = ((gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) ? 0x01:0x00);//Response buffer index
                                          //for polling BUSY bit. For Silicon rev A0: 0x00 and for rev B0: 0x01
    byBuffer[8] = ULBPR;// Unblock Global Protection instruction

    // if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,9,9)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //WREN
    byBuffer[0] = WREN;
    //performs write operation to the SPI Interface.
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //Chip Erase
    // byBuffer[0] = CHIP_ERASE;

    byBuffer[0] = 'S'; //Signature
    byBuffer[1] = 'P'; //Signature
    byBuffer[2] = 'I'; //Signature
    byBuffer[3] = 'D'; //Signature
    byBuffer[4] = 0x01; //Busy Bit Mask
    byBuffer[5] = RDSR; //Read Status opcode
    byBuffer[6] = 0x00; //No of dummy bytes
    byBuffer[7] = ((gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) ? 0x01:0x00);//Response buffer index
                                          //for polling BUSY bit. For Silicon rev A0: 0x00 and for rev B0: 0x01
    byBuffer[8] = CHIP_ERASE; //Chip Erase Instruction

    printf("\nErasing the SPI Flash...");
    // if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,1,1)) //write
    if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,9,9)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }
    // printf("\nErasing the SPI Flash...");

    // //Busy wait on the erase operation
    // byBuffer[0] = RDSR;
    // byBuffer[1] = 0;
    // do
    // {
    //     //performs write operation to the SPI Interface.
    //     if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,2,3)) //write
    //     {
    //         printf("SPI Transfer write failed \n");
    //         exit (1);
    //     }
    //     if(FALSE == MchpUsbSpiTransfer(DevID,1,(UINT8 *)&byReadBuffer,0,2))
    //     {
    //         printf("SPI Transfer read failed \n");
    //         exit (1);
    //     }
    // //}while((byReadBuffer[0] & 0x80)>>7);
    // }while(byReadBuffer[1] & 0x01);
    printf("DONE\n");

    NumPageWrites = BytesToWrite / WRITE_BLOCK_SIZE;
    RemainderBytes = BytesToWrite % WRITE_BLOCK_SIZE;

    byWriteBuffer[0] = 'S'; //Signature
    byWriteBuffer[1] = 'P'; //Signature
    byWriteBuffer[2] = 'I'; //Signature
    byWriteBuffer[3] = 'D'; //Signature
    byWriteBuffer[4] = 0x01; //Busy Bit Mask
    byWriteBuffer[5] = RDSR; //Read Status opcode
    byWriteBuffer[6] = 0x00; //No of dummy bytes
    byWriteBuffer[7] = ((gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) ? 0x01:0x00);//Response buffer index
                                          //for polling BUSY bit. For Silicon rev A0: 0x00 and for rev B0: 0x01
    byWriteBuffer[8] = PAGE_PROG; //Page Program Instruction

    for (uint16_t i=0; i<=NumPageWrites; i++)
    {

        // byWriteBuffer[1] = (StartAddr & 0xFF0000) >> 16;
        // byWriteBuffer[2] = (StartAddr & 0x00FF00) >> 8;
        // byWriteBuffer[3] = StartAddr & 0x0000FF;


        byWriteBuffer[9] = (StartAddr & 0xFF0000) >> 16;
        byWriteBuffer[10] = (StartAddr & 0x00FF00) >> 8;
        byWriteBuffer[11] = StartAddr & 0x0000FF;

        //WREN
        byBuffer[0] = WREN;
        //performs write operation to the SPI Interface.
        if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
        {
            printf("SPI Transfer write failed \n");
            exit (1);
        }

        DEBUGPRINT("Writing page %d at addr 0x%06x\n",i,StartAddr);

        if(i == NumPageWrites)
        {
            /*Copying the remaining binary data into write buffer when data lenth < WRITE_BLOCK_SIZE bytes*/
            // memcpy((void *)&byWriteBuffer[4], (const void *)&OutputData[i*WRITE_BLOCK_SIZE], RemainderBytes);
            memcpy((void *)&byWriteBuffer[12], (const void *)&OutputData[i*WRITE_BLOCK_SIZE], RemainderBytes);

            /*Writing a remaining bytes in the last page*/
            // if(FALSE == MchpUsbSpiTransfer(DevID,0,byWriteBuffer,RemainderBytes+4,RemainderBytes+4)) //write
            if(FALSE == MchpUsbSpiTransfer(DevID,0,byWriteBuffer,RemainderBytes+12,RemainderBytes+12)) //write
            {
                printf("SPI Transfer write failed \n");
                exit (1);
            }

        }
        else
        {
            /*Copying a page length of binary data into write buffer*/
            // memcpy((void *)&byWriteBuffer[4], (const void *)&OutputData[i*WRITE_BLOCK_SIZE],
            //                                                                 WRITE_BLOCK_SIZE);
            memcpy((void *)&byWriteBuffer[12], (const void *)&OutputData[i*WRITE_BLOCK_SIZE],
                                                                            WRITE_BLOCK_SIZE);

            /*Writing a WRITE_BLOCK_SIZE byte page*/
            // if(FALSE == MchpUsbSpiTransfer(DevID,0,byWriteBuffer,WRITE_BLOCK_SIZE+4,WRITE_BLOCK_SIZE+4)) //write
            if(FALSE == MchpUsbSpiTransfer(DevID,0,byWriteBuffer,WRITE_BLOCK_SIZE+12,WRITE_BLOCK_SIZE+12)) //write
            {
                printf("SPI Transfer write failed \n");
                exit (1);
            }
        }

        // //Check if the flash is BUSY
        // byBuffer[0] = RDSR;
        // byBuffer[1] = 0;
        // do
        // {
        //
        //     //performs write operation to the SPI Interface.
        //     if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],2,3)) //write
        //     {
        //         printf("SPI Transfer write failed \n");
        //         exit (1);
        //     }
        //
        //     if(FALSE == MchpUsbSpiTransfer(DevID,1,(UINT8 *)&byReadBuffer,0,2))
        //     {
        //         printf("SPI Transfer read failed \n");
        //         exit (1);
        //     }
        //
        //     // DEBUGPRINT("Writing page %d at addr 0x%06x...SR = %02x\n",i,StartAddr,byReadBuffer[0]);
        //     DEBUGPRINT("Writing page %d at addr 0x%06x...SR = %02x\n",i,StartAddr,byReadBuffer[1]);
        //
        // //}while((byReadBuffer[0] & 0x80)>>7);
        // }while(byReadBuffer[1] & 0x01);

        StartAddr += WRITE_BLOCK_SIZE;

        //Printing process completion
        if(i == 0)
        {
            printf("Programming SPI Flash...\n\n0%% ");
        }
        if(i%10 == 0)
        {
            printf("#");
        }
        if(i == NumPageWrites)
        {
            printf(" DONE\n\n");
        }
    }

    //WRDI
    byBuffer[0] = WRDIS;
    //performs write operation to the SPI Interface.
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
    {
        printf("SPI Transfer write failed \n");
        exit (1);
    }

    //Disable the SPI interface.
    if(FALSE == MchpUsbSpiSetConfig (DevID,0))
    {
        printf ("Error: SPI Pass thru enter failed:\n");
        exit (1);
    }

    //Reading the SPI Flash to compare it against the programmed binary file for
    //verification
    printf("Verifying the flash...\n");
    //Performs read operation from SPI Flash.
    if(FALSE == MchpUsbSpiFlashRead(DevID,0,byVerifyBuffer, BytesToWrite))
    {
        printf ("SPI Flash Verification Read Failed\n");
        exit (1);
    }

    if(0 == memcmp(byVerifyBuffer, OutputData, BytesToWrite))
	{
		printf("Flash Write Verification: Passed\n");
		return TRUE;
	}
	else
	{
		printf("Flash Write Verification: Failed\n");

        #ifdef DEBUG
              DEBUGPRINT("Generating flash_verify_fail.bin for debugging\n");
		      // writeBinfile("flash_verify_fail.bin", byVerifyBuffer, MAX_FW_SIZE);
              writeBinfile("flash_verify_fail.bin", byVerifyBuffer, BytesToWrite);
        #endif

        printf("Invalidating the SPI Flash signature...\n");

        //Enable the SPI interface.
        if(FALSE == MchpUsbSpiSetConfig (DevID,1))
        {
            printf ("\nError: SPI Pass thru enter failed:\n");
            exit (1);
        }

        //WREN
        byBuffer[0] = WREN;
        //performs write operation to the SPI Interface.
        if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,1)) //write
        {
            printf("SPI Transfer write failed \n");
            exit (1);
        }

        //Erasing the 32 Kb Block at 0x3C000  containg the '2DFU' signature
        // byBuffer[0] = CHIP_ERASE;

        byBuffer[0] = 'S'; //Signature
        byBuffer[1] = 'P'; //Signature
        byBuffer[2] = 'I'; //Signature
        byBuffer[3] = 'D'; //Signature
        byBuffer[4] = 0x01; //Busy Bit Mask
        byBuffer[5] = RDSR; //Read Status opcode
        byBuffer[6] = 0x00; //No of dummy bytes
        byBuffer[7] = ((gasHubInfo[DevID].sHubInfo.byDeviceRevision == 0xB0) ? 0x01:0x00);//Response buffer index
                                              //for polling BUSY bit. For Silicon rev A0: 0x00 and for rev B0: 0x01
        byBuffer[8] = BLOCK_ERASE; //Chip Erase Instruction
        byBuffer[9] = 0x03;
        byBuffer[10] = 0xC0;
        byBuffer[11] = 0x00;

        printf("Erasing the Block at 0x3C000...");
        // if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,1,1)) //write
        if(FALSE == MchpUsbSpiTransfer(DevID,0,byBuffer,12,12)) //write
        {
            printf("SPI Transfer write failed \n");
            exit (1);
        }
        printf("DONE\n");

        return FALSE;
	}
}

BOOL MchpUsbSpiTransfer(HANDLE DevID,INT Direction,UINT8* Buffer, UINT16 DataLength,UINT32 TotalLength)
{
	int bRetVal = FALSE;
	if(nullptr == Buffer)
	{
		DEBUGPRINT("SPI Transfer failed: NULL pointer");
		return FALSE;
	}

	if(Direction) //Read
	{
		bRetVal = libusb_control_transfer((libusb_device_handle*)gasHubInfo[DevID].handle,0xC1,
                    0x04,0x2310,0xBFD2,Buffer,TotalLength,CTRL_TIMEOUT);
	}
	else //Write
	{
        //Checking for SPI signature to enable non-zero wValue transfers
        if(Buffer[0] == 'S' && Buffer[1] =='P'&& Buffer[2] == 'I' && Buffer[3] == 'D' )
        {
            bRetVal = libusb_control_transfer((libusb_device_handle*)gasHubInfo[DevID].handle,0x41,
                        CMD_SPI_PASSTHRU_WRITE,TotalLength,0x0400,Buffer,DataLength,CTRL_TIMEOUT);
        }
        else
        {
            bRetVal = libusb_control_transfer((libusb_device_handle*)gasHubInfo[DevID].handle,0x41,
                CMD_SPI_PASSTHRU_WRITE,TotalLength,0,Buffer,DataLength,CTRL_TIMEOUT);
        }
	}
	if(bRetVal <0 )
	{
		DEBUGPRINT("SPI Transfer Failed\n");
		return FALSE;
	}
	else
	{
		DEBUGPRINT("SPI Transfer success\n");
		return TRUE;
	}
}

//Reads the JEDEC ID of the SPI Flash
BOOL GetJEDECID(HANDLE DevID, uint8_t *byBuffer)
{
    byBuffer[0] = READ_JEDEC_ID;
    if(FALSE == MchpUsbSpiTransfer(DevID,0,&byBuffer[0],1,4)) //write
    {
        DEBUGPRINT("SPI Transfer write failed \n");
        return FALSE;
    }

    if(FALSE == MchpUsbSpiTransfer(DevID,1,(UINT8 *)&byBuffer[0],0,4))
    {
        DEBUGPRINT("SPI Transfer write failed \n");
        return FALSE;
    }

    DEBUGPRINT("SPI Flash JEDEC ID read as %02x %02x %02x %02x\n",byBuffer[0],
        byBuffer[1], byBuffer[2], byBuffer[3]);

    return TRUE;
}

uint8_t ForceBootFromRom(HANDLE handle)
{
    uint8_t bRetVal = FALSE;
    uint8_t abyBuffer[4] = {'D','S','P','I'};

    /*
        For Silicon Rev A0: Write disable SPI signature 'D''S''P''I" to location
        0xBFD2_27EC
    */
    //Writing the signature to disable SPI ROM - Silicon Rev B1
    bRetVal = xdata_write(handle, 0xBFD227EC, abyBuffer, sizeof(abyBuffer));
    if(FALSE == bRetVal)
    {
        printf("Disable SPI signature write failed\n");
        return bRetVal;
    }

    //Issuing a Soft RESET
    abyBuffer[0] = BOOT_FROM_ROM_ON_RESET;
    xdata_write(handle, 0xBFD1DA1C, abyBuffer, 1);

    if(FALSE == bRetVal)
    {
        printf("Force Boot from ROM failed\n");
        return bRetVal;
    }

    /*Resetting the hub*/
    bRetVal = usb_reset_device(handle);
    if(FALSE == bRetVal)
    {
        printf("Failed to Reset the hub\n");
        return bRetVal;
    }

    //To allow time for the hub to boot up before performing another operation
    sleep(3);

    return bRetVal;
}

/*----------------------- Helper functions -----------------------------------*/
static int usb_get_hubs(PHINFO pHubInfoList)
{
	int cnt = 0, hubcnt = 0, i = 0, error=0;
	libusb_device **devs;
	libusb_device_descriptor desc;
	libusb_device_handle *handle;
	PHINFO pHubListHead = pHubInfoList;	// Pointer to head of the list

	error = libusb_init(&ctx);
	if(error < 0)
	{
		DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: Initialization LibUSB failed\n");
		return -1;
	}

	cnt = libusb_get_device_list(ctx, &devs);
	if(cnt < 0)
	{
		DEBUGPRINT("Failed to get the device list \n");
		return -1;
	}


	for (i = 0; i < cnt; i++)
	{
		int error = 0;
		int value = 0;

		libusb_device *device = devs[i];

		error = libusb_get_device_descriptor(device, &desc);
		if(error != 0)
		{
			DEBUGPRINT("LIBUSB_ERROR: Failed to retrieve device descriptor for device[%d] \n", i);
		}


		if((error ==  0) && (desc.bDeviceClass == LIBUSB_CLASS_HUB))
		{
			uint8_t hub_desc[ 7 /* base descriptor */
							+ 2 /* bitmasks */ * HUB_STATUS_BYTELEN];

		  	error = libusb_open(device, &handle);
			if(error < 0)
			{
				DEBUGPRINT("Cannot open device[%d] \t", i);
				switch(error)
				{
					case LIBUSB_ERROR_NO_MEM:
						DEBUGPRINT("LIBUSB_ERROR_NO_MEM \n");
					break;
					case LIBUSB_ERROR_ACCESS:
						DEBUGPRINT("LIBUSB_ERROR_ACCESS \n");
					break;
					case LIBUSB_ERROR_NO_DEVICE:
						DEBUGPRINT("LIBUSB_ERROR_NO_DEVICE \n");
					break;
					default:
						DEBUGPRINT("UNKNOWN_LIBUSB_ERROR %x\n", error);
					break;
				}
				continue;
			}
		 	memset(hub_desc, 0, 9);


			if(desc.bcdUSB == 0x0300)
			{
				value = 0x2A;
			}
		 	else
			{
		  		value = 0x29;
			}

			error = libusb_control_transfer(handle,
											LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
											LIBUSB_REQUEST_GET_DESCRIPTOR,
											value << 8, 0, hub_desc, sizeof hub_desc, CTRL_TIMEOUT
											);

		  	if(error < 0)
			{
				DEBUGPRINT("libusb_control_transfer device[%d]: \t", i);
				switch(error)
				{
					case LIBUSB_ERROR_TIMEOUT:
						DEBUGPRINT("LIBUSB_ERROR_TIMEOUT \n");
					break;
					case LIBUSB_ERROR_PIPE:
						DEBUGPRINT("LIBUSB_ERROR_PIPE \n");
					break;
					case LIBUSB_ERROR_NO_DEVICE:
						DEBUGPRINT("LIBUSB_ERROR_NO_DEVICE \n");
					break;
					default:
						DEBUGPRINT("UNKNOWN_LIBUSB_ERROR \n");
					break;
				}
				libusb_close(handle);
				continue;
			}

			pHubInfoList->port_max = libusb_get_port_numbers(device, pHubInfoList->port_list, 7);

		  	if(pHubInfoList->port_max <= 0)
			{
				libusb_close(handle);
				continue;
			}
			pHubInfoList->byPorts	= hub_desc[3];
			libusb_close(handle);

			pHubInfoList->wVID	 	= desc.idVendor;
			pHubInfoList->wPID 		= desc.idProduct;

			pHubInfoList++;
			hubcnt++;
		}
	}

	libusb_free_device_list(devs, 1);

	qsort(pHubListHead, hubcnt, sizeof(HINFO), compare_hubs);

	return hubcnt;
}

static int usb_get_hub_list(char *pHubInfoList)
{
	int cnt = 0,i = 0, error=0, port_cnt = 0, nportcnt = 0;
	libusb_device **devs;
	//libusb_device_handle *handle;
	libusb_device_descriptor desc;
//	PHINFO pHubListHead = pHubInfoList;	// Pointer to head of the list
	uint8_t port_list[7];

	*pHubInfoList = '\0';

	error = libusb_init(&ctx);

	if(error < 0)
	{
		DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: Initialization LibUSB failed\n");
		return -1;
	}


	cnt = libusb_get_device_list(ctx, &devs);
	if(cnt < 0)
	{
		DEBUGPRINT("Failed to get the device list \n");
		return -1;
	}
	for (i = 0; i < cnt; i++)
	{
		int error = 0;

		libusb_device *device = devs[i];
		error = libusb_get_device_descriptor(device, &desc);

		if(error != 0)
		{
			DEBUGPRINT("LIBUSB_ERROR: Failed to retrieve device descriptor for device[%d] \n", i);
		}

		if((error ==  0) && (desc.bDeviceClass == LIBUSB_CLASS_HUB))
		{
			port_cnt = libusb_get_port_numbers(device, port_list, 7);

			if(port_cnt < 1)
			{
				continue;
			}

			nportcnt++;

			char dbgmsg[50];
			memset(dbgmsg,0,50);

			sprintf(pHubInfoList,"%sVID:PID = %04x:%04x", pHubInfoList, desc.idVendor, desc.idProduct);
			//pirintf("VID:PID = %04x:%04x", desc.idVendor, desc.idProduct);
			for(int j = 0; j < port_cnt; j++)
			{

				sprintf(dbgmsg,"%s:%d",dbgmsg,port_list[j]);

			}
			sprintf(pHubInfoList, "%s, Device Path - %s\n",pHubInfoList, dbgmsg);

		}
	}
	printf("%s\n", pHubInfoList);
	libusb_free_device_list(devs, 1);
	return nportcnt;
}

static int compare_hubs(const void *p1, const void *p2)
{
	PHINFO pHub1, pHub2;

	pHub1 = (PHINFO) p1;
	pHub2 = (PHINFO) p2;


	if((VID_MICROCHIP == pHub1->wVID) && (VID_MICROCHIP == pHub2->wVID))
	{
		return 0; 	//Both Microchip hubs
	}
	else if (VID_MICROCHIP == pHub1->wVID)
	{
		return -1;	//Hub 1 is MCHP
	}
	else if (VID_MICROCHIP == pHub2->wVID)
	{
		return 1;  //Hub 2 is MCHP
	}

	return 0;
}

static int usb_open_HCE_device(uint8_t hub_index)
{
	libusb_device_handle *handle= NULL;
	libusb_device **devices;
	libusb_device *dev;
	libusb_device_descriptor desc;

	int dRetval = 0;
	ssize_t devCnt = 0, port_cnt = 0;
	ssize_t i = 0, j = 0, k=0;
	uint8_t port_list[7];

	devCnt = libusb_get_device_list(ctx, &devices);
	if(devCnt < 0)
	{
		DEBUGPRINT("Enumeration failed \n");
		return -1;
	}


	for (i = 0; i < devCnt; i++)
	{
		dev = devices[i];

		dRetval = libusb_get_device_descriptor(dev, &desc);
		if(dRetval < 0)
		{
			DEBUGPRINT("Cannot get the device descriptor \n");
			continue;
		}

        for(k = 0; k < HUB_SKUs; k++)
        {
    		if(PID_HCE_DEVICE[k] == desc.idProduct)
            // if(PID_HCE_DEVICE == desc.idProduct)
    		{
    			dRetval = libusb_open(dev, &handle);
    			if(dRetval < 0)
    			{
    				DEBUGPRINT("HCE Device open failed \n");
    				continue;
    			}

    			port_cnt = libusb_get_port_numbers(dev, port_list, 7);
    			if(port_cnt <= 1)
    			{
    				DEBUGPRINT("Retrieving port numbers failed \n");
    				libusb_close(handle);
    				continue;
    			}

    			//it is comapring against hub port count not HCE
    			if(gasHubInfo[hub_index].port_max != (port_cnt-1))
    			{
    				DEBUGPRINT("Hub port match failed with Hub the Index:%d\n",hub_index);
    				libusb_close(handle);
                                    continue;

    			}

    			//Match with the hub port list
    			for(j = 0; j < gasHubInfo[hub_index].port_max; j++)
    			{
    				if(gasHubInfo[hub_index].port_list[j] != port_list[j])
    				{
    					DEBUGPRINT("Hub port match failed with Hub Index:%d\n",hub_index);
    					dRetval = -1;
    					break;
    				}
    			}

    			if(dRetval == -1)
    			{
    				libusb_close(handle);
    				continue;
    			}

    			printf("HCE Hub index=%d Path- ",hub_index);
    			for(j = 0; j < port_cnt; j++)
    			{
    				printf(":%d", (unsigned int)(port_list[j]));
    			}
    			printf("\n");


    			if(libusb_kernel_driver_active(handle, 0) == 1)
    			{
    				//DEBUGPRINT("Kernel has attached a driver, detaching it \n");
    				if(libusb_detach_kernel_driver(handle, 0) != 0)
    				{
    					DEBUGPRINT("Cannot detach kerenl driver. USB device may not respond \n");
    					libusb_close(handle);
    					break;
    				}
    			}

    			dRetval = libusb_claim_interface(handle, 0);

    			if(dRetval < 0)
    			{
    				DEBUGPRINT("cannot claim intterface \n");
    				dRetval = -1;
    				libusb_close(handle);
    				break;
    			}

    			gasHubInfo[hub_index].dev = devices;
    			gasHubInfo[hub_index].handle = handle;
    			gasHubInfo[hub_index].byHubIndex = hub_index;
    			libusb_free_device_list(devices, 1);
    			return dRetval;
    		}
        }
	}

//	libusb_close(handle);

	libusb_free_device_list(devices, 1);
	return -1;
}

int usb_enable_HCE_device(uint8_t hub_index)
{
	libusb_device_handle *handle;
	libusb_device **devices;
	libusb_device *dev;
	libusb_device_descriptor desc;
	uint8_t port_list[7];


	int dRetval = 0;
	ssize_t devCnt = 0, port_cnt;
	ssize_t i = 0, j = 0;

	devCnt = libusb_get_device_list(ctx, &devices);
	if(devCnt <= 0)
	{
		DEBUGPRINT("Enumeration failed \n");
		return -1;
	}

	for (i = 0; i < devCnt; i++)
	{
		dev = devices[i];

		dRetval = libusb_get_device_descriptor(dev, &desc);
		if(dRetval < 0)
		{
			DEBUGPRINT("Cannot get the device descriptor \n");
			libusb_free_device_list(devices, 1);
			return -1;
		}

		if(MICROCHIP_HUB_VID == desc.idVendor)
		{
			dRetval = libusb_open(dev, &handle);
			if(dRetval < 0)
			{
				DEBUGPRINT("HCE Device open failed \n");
				continue;
			}


			port_cnt = libusb_get_port_numbers(dev, port_list, 7);
                        if(port_cnt < 1)
                        {
                                DEBUGPRINT("Retrieving port numbers failed \n");
				libusb_close(handle);
                                continue;
                        }

			//It is comaparing agaist hub port count not HCE
			if(gasHubInfo[hub_index].port_max != (port_cnt))
                        {
                                DEBUGPRINT("Hub port match failed with Hub Index:%d\n",hub_index);
                                libusb_close(handle);
                                continue;

                        }

                        for(j = 0; j < gasHubInfo[hub_index].port_max; j++)
                        {
                                if(gasHubInfo[hub_index].port_list[j] != port_list[j])
                                {
                                        DEBUGPRINT("Hub port match failed \n");
                                        dRetval = -1;
                                        break;
                                }
                        }

                        if(dRetval == -1)
                        {
                        	libusb_close(handle);
				continue;
                        }

			printf("HCE capable hub found at Hub index=%d Path- ", hub_index);

                        for(i = 0; i < port_cnt; i++)
                        {
                                printf(":%d", (unsigned int)(port_list[i]));
                        }
                        printf("\n");


			if(libusb_kernel_driver_active(handle, 0) == 1)
			{

				if(libusb_detach_kernel_driver(handle, 0) != 0)
				{
					DEBUGPRINT("Cannot detach kerenl driver. USB device may not respond \n");
					libusb_close(handle);
					break;
				}
			}

			dRetval = libusb_claim_interface(handle, 0);

			if(dRetval < 0)
			{
				DEBUGPRINT("cannot claim intterface \n");
				libusb_close(handle);
				break;
			}

			uint16_t val = 0x0001;
			dRetval = usb_send_vsm_command(handle,(uint8_t*)&val);
			if(dRetval < 0)
			{
				DEBUGPRINT("HCE Device: VSM command 0x0001 failed \n");
				libusb_close(handle);
				break;
			}

			val = 0x0201;
			dRetval = usb_send_vsm_command(handle,(uint8_t*)&val);
			if(dRetval < 0)
			{
				DEBUGPRINT("HCE Device: VSM command 0x0201 failed \n");
			}
			sleep(2);

			libusb_close(handle);
			libusb_free_device_list(devices, 1);
			return dRetval;
		}
	}

	libusb_free_device_list(devices, 1);
	return -1;
}

int usb_reset_device(HANDLE handle)
{
    int bRetVal = FALSE;
    USB_CTL_PKT UsbCtlPkt;

    UsbCtlPkt.handle 	= (libusb_device_handle*)gasHubInfo[handle].handle;
    UsbCtlPkt.byRequest = CMD_DEV_RESET;
    UsbCtlPkt.wValue 	= 0x0001;
    UsbCtlPkt.wIndex 	= 0;
    UsbCtlPkt.byBuffer 	= 0;
    UsbCtlPkt.wLength 	= 0;

    bRetVal = usb_HCE_write_data (&UsbCtlPkt);
	if(bRetVal< 0)
	{
		DEBUGPRINT("Device Reset failed %d\n",bRetVal);
		return FALSE;
	}
    return TRUE;
}

int get_hub_info(HANDLE handle, uint8_t *data)
{
	int bRetVal = FALSE;
	USB_CTL_PKT UsbCtlPkt;

	UsbCtlPkt.handle 	= (libusb_device_handle*)gasHubInfo[handle].handle;
	UsbCtlPkt.byRequest = 0x09;
	UsbCtlPkt.wValue 	= 0;
	UsbCtlPkt.wIndex 	= 0;
	UsbCtlPkt.byBuffer 	= data;
	UsbCtlPkt.wLength 	= 6;
	bRetVal = usb_HCE_read_data (&UsbCtlPkt);
	if(bRetVal< 0)
	{
		DEBUGPRINT("Execute HubInfo command failed %d\n",bRetVal);
		return bRetVal;
	}
	return bRetVal;
}

int  usb_send_vsm_command(struct libusb_device_handle *handle, uint8_t * byValue)
{
	int rc = 0;

	rc = libusb_control_transfer(	handle,
					0x40,
					0x02,
					0,
					0,
					byValue,
					2,
					CTRL_TIMEOUT
				);
	return rc;
}

int xdata_read(HANDLE handle, uint32_t wAddress, uint8_t *data, uint8_t num_bytes)
{
	int bRetVal = FALSE;
	USB_CTL_PKT UsbCtlPkt;

	UsbCtlPkt.handle 	= (libusb_device_handle*)gasHubInfo[handle].handle;
	UsbCtlPkt.byRequest 	= 0x04;
	UsbCtlPkt.wValue 	= (wAddress & 0xFFFF);
	UsbCtlPkt.wIndex 	= ((wAddress & 0xFFFF0000) >> 16);
	UsbCtlPkt.byBuffer 	= data;
	UsbCtlPkt.wLength 	= num_bytes;

	bRetVal = usb_HCE_read_data (&UsbCtlPkt);
	return bRetVal;
}

int xdata_write(HANDLE handle, uint32_t wAddress, uint8_t *data, uint8_t num_bytes)
{
	int bRetVal = FALSE;
	USB_CTL_PKT UsbCtlPkt;

	UsbCtlPkt.handle 	= (libusb_device_handle*)gasHubInfo[handle].handle;
	UsbCtlPkt.byRequest = 0x03;
	UsbCtlPkt.wValue 	= (wAddress & 0xFFFF);
	UsbCtlPkt.wIndex 	= ((wAddress & 0xFFFF0000) >> 16);
	UsbCtlPkt.byBuffer 	= data;
	UsbCtlPkt.wLength 	= num_bytes;

	bRetVal = usb_HCE_write_data (&UsbCtlPkt);
	return bRetVal;
}

//Return handle to the first instance of VendorID &amp; ProductID matched device.
//VID and PID of Hub Feature Controller - by dealut vid:pid - 0x424:0x2530
//This API shouldn't use if there is multiple HUBS with HFC(vid:pid - 0x424:0x2530)
HANDLE  MchpUsbOpenHFC (UINT16 wVID, UINT16 wPID)
{
	int error = 0;
	libusb_device_handle *handle;

	error = libusb_init(&ctx);
	if(error < 0)
	{
		DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: Initialization LibUSB failed\n");
		return -1;
	}


	handle = libusb_open_device_with_vid_pid(ctx, wVID, wPID);
	if(NULL == handle)
	{
		DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: Failed to open the HFC vid:pid - 0x%04x:0x%04x\n", wVID, wPID);
		return -1;
	}

	/*Check if kenel driver attached*/
	if(libusb_kernel_driver_active(handle, 0))
   	{
		error = libusb_detach_kernel_driver(handle, 0); // detach driver

		if(0 > error)
		{
			DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: libusb_detach_kernel_driver failed\n");
			libusb_close(handle);
			return -1;
		}

   	}
   	error = libusb_claim_interface(handle, 0);
   	if(0 > error)
	{
		DEBUGPRINT("MCHP_Error_LibUSBAPI_Fail: libusb_claim_interface failed\n");
		libusb_close(handle);
		return -1;
	}

	gasHubInfo[0].handle = handle;
	gasHubInfo[0].byHubIndex = 0;

	return 0;
}
