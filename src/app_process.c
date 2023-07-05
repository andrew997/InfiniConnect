/***************************************************************************//**
 * @file
 * @brief app_process.c
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
 * freely, subject to the following restrictions:
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "hal/hal.h"
#include "em_chip.h"
#include "app_log.h"
#include "app_framework_common.h"

#include "app_assert.h"

#include "app_init.h"
#include "em_usart.h"
#include "em_timer.h"
#include "em_cmu.h"
#include "em_wdog.h"
#include "event.h"


// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define DATA_ENDPOINT           1
#define TX_TEST_ENDPOINT        2
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
void sendTxTestPacket(void);
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
bool tx_test_print_en = false;

extern EmberMessageOptions tx_options;


// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------


static volatile int pkt_pointer = 0;

// The buffer used to hold the incoming Connect packet
uint8_t rx_pkt[TX_BUF_SIZE];
int rx_pkt_size = 0;

// the buffer used to hold the packet coming from the carrier unit
uint8_t tx_pkt [TX_BUF_SIZE];
int tx_pkt_size = 0;

struct  __attribute__((__packed__))  {
  //uint8_t size;
  uint8_t last_pkt;
  uint8_t payload[MAX_CONNECT_PKT];
} connect_pkt, connect_rx_pkt;

extern EmberEventControl *packetProcess, *wdogProcess;



// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

void wdogEventHandler(void) {

  emberEventControlSetDelayMS(*wdogProcess, WDOG_TIMER_MS);

  // Feed the watchdog
  WDOGn_Feed(DEFAULT_WDOG);
}

void packetEventHandler(void) {

  int i = 0;
  EmberStatus status;

  emberEventControlSetInactive(*packetProcess);

  //GPIO_PinOutClear(gpioPortC, 3);   // For Debug

#ifdef FURNACE
    // Check if destination address is 0x52 - the outdoor unit
    // if not, exit
    if (tx_pkt[0] != 0x52) {
        // reset counters/pointers
        tx_pkt_size = 0;
        NVIC_ClearPendingIRQ(USART0_RX_IRQn);
        NVIC_EnableIRQ(USART0_RX_IRQn);
        return;
    }
#endif

    while (tx_pkt_size > 0) {
        //connect_pkt.size = tx_pkt_size > MAX_CONNECT_PKT ? MAX_CONNECT_PKT : tx_pkt_size;
        connect_pkt.last_pkt = tx_pkt_size > MAX_CONNECT_PKT ? 0 : 1;
        memcpy(connect_pkt.payload, &tx_pkt[i*MAX_CONNECT_PKT], tx_pkt_size > MAX_CONNECT_PKT ? MAX_CONNECT_PKT : tx_pkt_size );

        status = emberMessageSend(DESTID,  // node ID
                        1,  // DATA_ENDPOINT
                        connect_pkt.last_pkt, // messageTag
                        tx_pkt_size > MAX_CONNECT_PKT ? MAX_CONNECT_PKT+1 : tx_pkt_size+1,  // message length
                        &connect_pkt.last_pkt,   // message
                        tx_options);
        if (status != EMBER_SUCCESS) {
            app_log_info("emberMessageSend Status: 0x%02d\n", status);
        }

        if (connect_pkt.last_pkt == 1) {  // Last packet in sequence
            break;

        }
        else {
            tx_pkt_size = tx_pkt_size - MAX_CONNECT_PKT;
            i++;
        }
    }

    tx_pkt_size = 0;
    NVIC_ClearPendingIRQ(USART0_RX_IRQn);
    NVIC_EnableIRQ(USART0_RX_IRQn);
}


/******************************************************************************
 * Application logic (Empty)
 *****************************************************************************/
void emberAfTickCallback(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your application logic code here!                                   //
  // This is called infinitel.                                               //
  /////////////////////////////////////////////////////////////////////////////


}

/******************************************************************************
 * This callback is called if a message is received. If CLI is added it
 * prints the incoming message.
 *****************************************************************************/
void emberAfIncomingMessageCallback(EmberIncomingMessage *message)
{
  int i;

  if (message->endpoint == DATA_ENDPOINT) {
      memcpy(&connect_rx_pkt, message->payload, message->length );
      memcpy(&rx_pkt[pkt_pointer*MAX_CONNECT_PKT], connect_rx_pkt.payload, message->length-1 );
      rx_pkt_size += (message->length-1);
      if (connect_rx_pkt.last_pkt == 1) {
          // Print / TX the entire thing
          app_log_info("RSSI: %d Size %d: ", message->rssi,  rx_pkt_size);
          for ( i = 0; i < rx_pkt_size ; i++ ) {
                            app_log_info("%02X ", rx_pkt[i]);
                            USART_Tx(USART0, rx_pkt[i]);
          }
          app_log_info("\n");
          rx_pkt_size = 0;
          pkt_pointer = 0;
      } else {
          // increment the pkt_pointer; more packets are inbound for this carrier frame
          pkt_pointer++;
      }
  }
}

/******************************************************************************
 * This callback is called if a message is transmitted. If CLI is added it
 * prints message success.
 *****************************************************************************/
void emberAfMessageSentCallback(EmberStatus status,
                                EmberOutgoingMessage *message)
{
  if (message->endpoint == DATA_ENDPOINT) {
    if ( status != EMBER_SUCCESS ) {
      app_log_info("TX: 0x%02X\n", status);
    }
  }

  // check the messageTag.  If it's 1, this callback is the last packet in
  // the sequence so re-enable UART interrupts
  if (message->tag == 1) {
    NVIC_ClearPendingIRQ(USART0_RX_IRQn);
    NVIC_EnableIRQ(USART0_RX_IRQn);
  }
}

/******************************************************************************
 * This callback is called if network status changes. If CLI is added it
 * prints the current status.
 *****************************************************************************/
void emberAfStackStatusCallback(EmberStatus status)
{
  switch (status) {
    case EMBER_NETWORK_UP:
      app_log_info("Network up\n");
      break;
    case EMBER_NETWORK_DOWN:
      app_log_info("Network down\n");
      break;
    default:
      app_log_info("Stack status: 0x%02X\n", status);
      break;
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * This function creates a packet with consecutive data and sends it
 *****************************************************************************/
void sendTxTestPacket(void)
{
  for (int i = 0; i < 400 ; i++) {
      tx_pkt[i] = i;
  }
  tx_pkt_size = 400;
  tx_pkt[0] = 0x52;

  emberEventControlSetActive(*packetProcess);
}

void emberAfEnergyScanCompleteCallback(int8_t mean,
                                       int8_t min,
                                       int8_t max,
                                       uint16_t variance)
{
  app_log_info("Energy scan complete, mean=%d min=%d max=%d var=%d\n",
               mean, min, max, variance);
}


// UARTDRV Callback
void USART0_RX_IRQHandler(void)
{
  tx_pkt[tx_pkt_size] = USART0->RXDATA;
  tx_pkt_size++;

  TIMER_CounterSet(TIMER1, 0);
  TIMER_Enable(TIMER1, true);
}

// TIMER Interrupt handler - it expired, so we have a full packet in the rx buffer to process
void TIMER1_IRQHandler(void){
  uint32_t flags = TIMER_IntGet(TIMER1);
  TIMER_IntClear(TIMER1, flags);
  TIMER_Enable(TIMER1, false);
  //GPIO_PinOutSet(gpioPortC, 3);  // debug
  NVIC_DisableIRQ(USART0_RX_IRQn);
  NVIC_ClearPendingIRQ(USART0_RX_IRQn);

  emberEventControlSetActive(*packetProcess);
 }

