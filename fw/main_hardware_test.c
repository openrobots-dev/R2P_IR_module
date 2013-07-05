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

#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "halconf.h"
#include "test.h"
#include "shell.h"
#include "chprintf.h"

#define WA_SIZE_1K      THD_WA_SIZE(1024)

#define ADC_NUM_CHANNELS 4
#define ADC_BUF_DEPTH 1

static adcsample_t adc_samples[ADC_NUM_CHANNELS * ADC_BUF_DEPTH] = {0};
uint16_t measure = 0;

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(4096)
#define TEST_WA_SIZE    THD_WA_SIZE(1024)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] =
    {THD_STATE_NAMES};
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n", (uint32_t)tp,
             (uint32_t)tp->p_ctx.r13, (uint32_t)tp->p_prio,
             (uint32_t)(tp->p_refs - 1), states[tp->p_state],
             (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(), TestThread,
                           chp);
  if (tp == NULL) {
    chprintf(chp, "out of memory\r\n");
    return;
  }
  chThdWait(tp);
}

static void cmd_en(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: en\r\n");
    return;
  }

  palClearPad(IREN_GPIO, IREN);
}

static void cmd_dis(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: dis\r\n");
    return;
  }

  palSetPad(IREN_GPIO, IREN);
}

static void cmd_measure(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: m\r\n");
    return;
  }

  if (measure) {
	  measure = 0;
  } else {
	  measure = 1;
  }
}

static const ShellCommand commands[] =
  {
    {"mem", cmd_mem},
     {"threads", cmd_threads},
     {"test", cmd_test},
     {"en", cmd_en},
     {"dis", cmd_dis},
     {"m", cmd_measure},
     {NULL, NULL}};

static const ShellConfig shell_cfg1 =
  {(BaseSequentialStream *)&SERIAL_DRIVER, commands};

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
  ADC_SQR3_SQ4_N(ADC_CHANNEL_IN9)   | ADC_SQR3_SQ3_N(ADC_CHANNEL_IN8) |
  ADC_SQR3_SQ2_N(ADC_CHANNEL_IN2)   | ADC_SQR3_SQ1_N(ADC_CHANNEL_IN1)
};

/*===========================================================================*/
/* Application threads.                                                      */
/*===========================================================================*/

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;

  chRegSetThreadName("blinker");
  while (TRUE) {
    palTogglePad(LED_GPIO, LED1);
    palTogglePad(LED_GPIO, LED2);
    palTogglePad(LED_GPIO, LED3);
    palTogglePad(LED_GPIO, LED4);
    chThdSleepMilliseconds(500);
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
   * Activates the serial driver 1 using the driver default configuration.
   */
  sdStart(&SERIAL_DRIVER, NULL);

  /*
   * Shell manager initialization.
   */
  shellInit();

	/*
	 * Activates the ADC1 driver.
	 */
	adcStart(&ADCD1, NULL);
	adcStartConversion(&ADC_DRIVER, &adc_grpcfg, adc_samples, ADC_BUF_DEPTH);

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (TRUE) {
    if (!shelltp)
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminated(shelltp)) {
      chThdRelease(shelltp);
      shelltp = NULL;
    }

    if (measure) {
    	chprintf((BaseSequentialStream *)&SERIAL_DRIVER, "M: %d %d %d %d\r\n", adc_samples[0], adc_samples[1], adc_samples[2], adc_samples[3]);
    }

    chThdSleepMilliseconds(200);
  }
}
