/***************************************************************************//**
 * @file
 * @brief app_init.c
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
#include "app_log.h"
#include "ember.h"
#include "hal.h"
#include "app_framework_common.h"
// Ensure that psa is initialized corretly
#include "psa/crypto.h"

// for USART
#include "em_gpio.h"
#include "em_usart.h"
#include "em_cmu.h"
#include "em_timer.h"

#include "app_init.h"

#include "em_wdog.h"




// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

static void initUSART0(void);
static void initTIMER1(void);
static void initWDOG(void);

extern bool set_security_key(uint8_t* key, size_t key_length);
extern void packetEventHandler(void);
extern void wdogEventHandler(void);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

extern uint8_t tx_pkt [TX_BUF_SIZE];
extern EmberMessageOptions tx_options;

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------


static uint8_t security_key[16] = {SECURITY_KEY};

EmberEventControl *packetProcess, *wdogProcess;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
* Application framework init callback
******************************************************************************/
void emberAfInitCallback(void)
{

  EmberNetworkParameters params;
  params.panId = PANID;
  params.radioChannel = CHANNEL;
  params.radioTxPower = TX_POWER;
  EmberNodeId nodeId = NODEID;

  // set TX options.  we want ACKs and security enabled
  tx_options = 0x03;

  // Ensure that psa is initialized corretly
  psa_crypto_init();

  // set security key
  set_security_key(security_key, sizeof(security_key));

  // CLI info message
  app_log_info("Direct Mode Device\n");

  //Initialize USART0
  initUSART0();

  //Enable USART0 RX interrupts
  USART_IntEnable(USART0, USART_IEN_RXDATAV);

  // Initialize TIMER1 for after-packet timer
  initTIMER1();

  // Initialize network
  EmberStatus status;
  status = emberNetworkInit();

  if (status == EMBER_NOT_JOINED) {

      // need to recover, force a join
      status = emberJoinCommissioned(EMBER_DIRECT_DEVICE, nodeId, &params);
      if (status != EMBER_SUCCESS) app_log_error("Commissioning failed, 0x%02X", status);
  }

  // network status
  status = emberNetworkState();
  app_log_info("NetworkState: %x\n", status);

  if (status == EMBER_JOINED_NETWORK) {
      // Initialize watchdog feeder event
      emberAfAllocateEvent(&wdogProcess, &wdogEventHandler);
      // start WDOG
      initWDOG();
      // Give the dog an initial feeding to start the event timer
      emberEventControlSetActive(*wdogProcess);
  } else app_log_error("Commissioning failed, 0x%02X", status);

  // initialize/clear packet buffers
  memset(tx_pkt, 0, sizeof(tx_pkt));

  // Initialize packet processing event
  emberAfAllocateEvent(&packetProcess, &packetEventHandler);



#if defined(EMBER_AF_PLUGIN_BLE)
  bleConnectionInfoTableInit();
#endif
}
// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

static void initUSART0(void) {

  // Initialize USART0
  CMU_ClockEnable(cmuClock_USART0, true);
  GPIO_PinModeSet(gpioPortC, 1, gpioModePushPull, 0);  // TX pin
  GPIO_PinModeSet(gpioPortC, 2, gpioModeInput, 0);  // RX pin

  GPIO_PinModeSet(gpioPortC, 3, gpioModePushPull, 0);  // debug

  // Default asynchronous initializer (115.2 Kbps, 8N1, no flow control)
   USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;
   init.baudrate = 38400;

   // Route USART0 TX and RX to the board controller TX and RX pins
   GPIO->USARTROUTE[0].TXROUTE = (gpioPortC << _GPIO_USART_TXROUTE_PORT_SHIFT)
       | (1 << _GPIO_USART_TXROUTE_PIN_SHIFT);
   GPIO->USARTROUTE[0].RXROUTE = (gpioPortC << _GPIO_USART_RXROUTE_PORT_SHIFT)
       | (2 << _GPIO_USART_RXROUTE_PIN_SHIFT);

   // Enable RX and TX signals now that they have been routed
   GPIO->USARTROUTE[0].ROUTEEN = GPIO_USART_ROUTEEN_RXPEN | GPIO_USART_ROUTEEN_TXPEN;

   // Configure and enable USART0
   USART_InitAsync(USART0, &init);

   // Enable NVIC USART sources
   NVIC_ClearPendingIRQ(USART0_RX_IRQn);
   NVIC_EnableIRQ(USART0_RX_IRQn);
}

// TIMER1 in xG23 is only 16-bits, not 32, so we have to prescale the clock
// TIMER0 is 32-bits but is reserved by the Connect stack

static void initTIMER1(void) {
  CMU_ClockEnable(cmuClock_TIMER1, true);

  // Initialize the timer
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
  timerInit.enable = false;

  // Timer clock = 39 MHz / 8 = 4.875 MHz
  timerInit.prescale = timerPrescale8;

  // Run in one-shot mode and toggle the pin on each compare match
  timerInit.oneShot = true;
  timerCCInit.mode = timerCCModeCompare;
  timerCCInit.cmoa = timerOutputActionNone;

  TIMER_Init(TIMER1, &timerInit);

  TIMER_TopSet(TIMER1, 0xFFFFFFFF);

  TIMER_InitCC(TIMER1, 0, &timerCCInit);

  // Set the first compare value
  TIMER_CompareSet(TIMER1, 0, 4875000 * TIMER_DELAY_MS / 1000);
  // Enable TIMER1 interrupts
  TIMER_IntEnable(TIMER1, TIMER_IEN_CC0);
  NVIC_EnableIRQ(TIMER1_IRQn);
}

void initWDOG(void)
{
  // Enable clock for the WDOG module; has no effect on xG21
  CMU_ClockEnable(cmuClock_WDOG0, true);

  // Watchdog Initialize settings
  WDOG_Init_TypeDef wdogInit = WDOG_INIT_DEFAULT;
  CMU_ClockSelectSet(cmuClock_WDOG0, cmuSelect_ULFRCO); /* ULFRCO as clock source */
  wdogInit.debugRun = true;
  wdogInit.em1Run = true;
  wdogInit.perSel = wdogPeriod_2k; // 2049 clock cycles of a 1kHz clock  ~2 seconds period

  // Initializing watchdog with chosen settings
  WDOGn_Init(DEFAULT_WDOG, &wdogInit);
}
