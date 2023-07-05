/***************************************************************************//**
 * @file
 * @brief app_init.h
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrict
 *
 * ions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#ifndef APP_INIT_H
#define APP_INIT_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

#define FURNACE 1  // This app is for the furnace

#define TX_BUF_SIZE             512
#define RX_BUF_SIZE             512
#define MAX_CONNECT_PKT         94

#define SECURITY_KEY  0x63, 0x44, 0xcc, 0x30, 0x80, 0xad, 0x65, 0x6e, 0xbe, 0x36, 0xb2, 0xa0, 0x1d, 0x1c, 0xcb, 0x46

#define TIMER_DELAY_MS  0.5

#define WDOG_TIMER_MS 2000

// Channel
#define CHANNEL 0x0002

// PAN ID
#define PANID 0x1362

// Node ID
#ifdef FURNACE
#define NODEID  0x0001
#define DESTID  0x0002
#else
#define NODEID  0x0002
#define DESTID 0x0001
#endif


// TX Power in deci-dBm
#define TX_POWER  100

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

#endif  // APP_INIT_H
