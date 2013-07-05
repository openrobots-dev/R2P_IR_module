/*
 ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
 2011 Giovanni Di Sirio.

 This file is part of ChibiOS/RT.

 ChibiOS/RT is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 ChibiOS/RT is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ch.h"
#include "hal.h"

#include "rtcan.h"
#include "Middleware.hpp"
#include "msg/r2p_ir.h"


#define WA_SIZE_2K      THD_WA_SIZE(2048)

#define ADC_NUM_CHANNELS 4
#define ADC_BUF_DEPTH 2

static adcsample_t adc_samples[ADC_NUM_CHANNELS * ADC_BUF_DEPTH] = {0};

RTCANConfig rtcan_config = {1000000, 100, 60};

/*===========================================================================*/
/* STM32 id & reset.                                                         */
/*===========================================================================*/

static uint8_t stm32_id8(void) {
	const unsigned long * uid = (const unsigned long *)0x1FFFF7E8;

	return (uid[2] & 0xFF);
}

static void stm32_reset(void) {

	chThdSleep(MS2ST(10) );

	/* Ensure completion of memory access. */
	__DSB();

	/* Generate reset by setting VECTRESETK and SYSRESETREQ, keeping priority group unchanged.
	 * If only SYSRESETREQ used, no reset is triggered, discovered while debugging.
	 * If only VECTRESETK is used, if you want to read the source of the reset afterwards
	 * from (RCC->CSR & RCC_CSR_SFTRSTF),
	 * it won't be possible to see that it was a software-triggered reset.
	 * */

	SCB ->AIRCR = ((0x5FA << SCB_AIRCR_VECTKEY_Pos)
			| (SCB ->AIRCR & SCB_AIRCR_PRIGROUP_Msk)| SCB_AIRCR_VECTRESET_Msk
			| SCB_AIRCR_SYSRESETREQ_Msk);

	/* Ensure completion of memory access. */
	__DSB();

	/* Wait for reset. */
	while (1)
		;
}

/*===========================================================================*/
/* ADC related.                                                              */
/*===========================================================================*/

/*
 * ADC conversion group.
 * Mode:        Linear buffer, 8 samples of 1 channel, SW triggered.
 * Channels:    IN10.
 */

static const ADCConversionGroup adc_grpcfg = {
  TRUE,
  ADC_NUM_CHANNELS,
  NULL,
  NULL,
  0, /* CR1 */
  0, /* CR2 */
  0, /* SMPR1 */
  ADC_SMPR2_SMP_AN9(ADC_SAMPLE_239P5) | ADC_SMPR2_SMP_AN8(ADC_SAMPLE_239P5) |
  ADC_SMPR2_SMP_AN2(ADC_SAMPLE_239P5) | ADC_SMPR2_SMP_AN1(ADC_SAMPLE_239P5),
  ADC_SQR1_NUM_CH(ADC_NUM_CHANNELS),
  0, /* SQR2 */
  ADC_SQR3_SQ4_N(ADC_CHANNEL_IN8)   | ADC_SQR3_SQ3_N(ADC_CHANNEL_IN9) |
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN2)   | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN1)
};

/*
 * Publisher threads.
 */
static msg_t PublisherThread1(void *arg) {
	Middleware & mw = Middleware::instance();
	Node n("IRRaw");
	Publisher<IRRaw> pub("IRRaw");
	IRRaw *d;
	systime_t time;

	(void) arg;
	chRegSetThreadName("PUB THD #1");

	mw.newNode(&n);

	if (! n.advertise(&pub)) {
		mw.delNode(&n);
		while(1);
	}

	// FIXME
	RemoteSubscriberT<IRRaw, 5> rsub("IRRaw");
	rsub.id(IRRAW_ID | stm32_id8());
	rsub.subscribe(&pub);

	chThdSleepMilliseconds(100);

	time = chTimeNow();

	while (TRUE) {
		d = pub.alloc();
		if (d) {
			d->value1 = adc_samples[0];
			d->value2 = adc_samples[1];
			d->value3 = adc_samples[2];
			d->value4 = adc_samples[3];
			pub.broadcast(d);
		}

		time += MS2ST(50);
		chThdSleepUntil(time);
	}

	return 0;
}

/*===========================================================================*/
/* Application threads.                                                      */
/*===========================================================================*/

/*
 * LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(wa_blinker_thread, 128);
static msg_t blinker_thread(void *arg) {

	(void) arg;
	chRegSetThreadName("blinker");

	while (TRUE) {
		switch (RTCAND1.state) {
		case RTCAN_MASTER:
			palClearPad(LED_GPIO, LED1);
			chThdSleepMilliseconds(200);
			palSetPad(LED_GPIO, LED1);
			chThdSleepMilliseconds(100);
			palClearPad(LED_GPIO, LED1);
			chThdSleepMilliseconds(200);
			palSetPad(LED_GPIO, LED1);
			chThdSleepMilliseconds(500);
			break;
		case RTCAN_SYNCING:
			palTogglePad(LED_GPIO, LED1);
			chThdSleepMilliseconds(100);
			break;
		case RTCAN_SLAVE:
			palTogglePad(LED_GPIO, LED1);
			chThdSleepMilliseconds(500);
			break;
		case RTCAN_ERROR:
			palTogglePad(LED_GPIO, LED4);
			chThdSleepMilliseconds(200);
			break;
		default:
			chThdSleepMilliseconds(100);
			break;
		}
	}

	return 0;
}

/*
 * Application entry point.
 */

int main(void) {
  Thread *shelltp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Activates the RTCAN driver.
   */
  chThdSleepMilliseconds(100);
  rtcanInit();
  rtcanStart(&RTCAND1, &rtcan_config);

  /*
   * Activates the ADC1 driver.
   */
  adcStart(&ADC_DRIVER, NULL);
  adcStartConversion(&ADC_DRIVER, &adc_grpcfg, adc_samples, ADC_BUF_DEPTH);

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(wa_blinker_thread, sizeof(wa_blinker_thread), NORMALPRIO, blinker_thread, NULL);


  /*
   * Enable IR sensors power supply.
   */
  palClearPad(IREN_GPIO, IREN);

  /*
   * Creates the IR publisher thread.
   */
  chThdCreateFromHeap(NULL, WA_SIZE_2K, NORMALPRIO + 1, PublisherThread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (TRUE) {
    chThdSleepMilliseconds(200);
  }
}
