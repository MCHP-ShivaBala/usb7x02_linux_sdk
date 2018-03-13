/*******************************************************************************
  MPLABConnect SDK header file

  Company:
    Microchip Technology Inc.

  File Name:
    USBHubAbstraction.h

  Summary:
    MPLABConnect SDK Header File for file handling and data transfer APIs

  Description:
    MPLABConnect SDK LIB Header file for file handling and data transfer APIs
*******************************************************************************/
// DOM-IGNORE-BEGIN
/*******************************************************************************

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

*******************************************************************************/
// DOM-IGNORE-END

#pragma once

#include "typedef.h"
#include "USBHubAbstraction.h"

/*ReadBinfile
 * API is used to read content of binary file*/
int32_t ReadBinfile(char const *name, uint8_t *buffer);

/*writeBinfile
 * API is used to write content to binary file*/
uint32_t writeBinfile(char const *name, uint8_t *buffer, unsigned long fileLen);

/*usb_HCE_read_data
 * API does libusb control transfer for Read operation*/
int  usb_HCE_read_data(PUSB_CTL_PKT pUsbCtlPkt);

/*usb_HCE_write_data
 * API does libusb control transfer for write operation*/
int  usb_HCE_write_data(PUSB_CTL_PKT pUsbCtlPkt);

/*usb_HCE_no_data
 * API is used to send no data*/
int  usb_HCE_no_data(PUSB_CTL_PKT pUsbCtlPkt);
