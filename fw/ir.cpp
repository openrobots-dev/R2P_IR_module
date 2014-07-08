#include "ch.h"
#include "hal.h"
#include "ir.h"

#include <r2p/Middleware.hpp>

const ADCConversionGroup adc_conf_1 = {
  FALSE,
  1,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN1(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(1),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN1)
};

const ADCConversionGroup adc_conf_2 = {
  FALSE,
  1,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN2(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(1),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN2)
};

const ADCConversionGroup adc_conf_3 = {
  FALSE,
  1,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN8(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(1),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN8)
};

const ADCConversionGroup adc_conf_4 = {
  FALSE,
  1,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN9(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(1),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN9)
};

msg_t irpub_node(void * arg) {
	irpub_conf * conf = reinterpret_cast<irpub_conf *>(arg);
	r2p::Node node("irpub");
	r2p::Publisher<ProximityMsg> ir_pub;
	const ADCConversionGroup * adc_conf;
	adcsample_t sample;

	if (conf == NULL) return CH_FAILED;

	switch (conf->channel) {
	case 1:
		palSetPadMode(GPIOA, 1, PAL_MODE_INPUT_ANALOG);
		adc_conf = &adc_conf_1;
		break;
	case 2:
		palSetPadMode(GPIOA, 2, PAL_MODE_INPUT_ANALOG);
		adc_conf = &adc_conf_2;
		break;
	case 3:
		palSetPadMode(GPIOB, 1, PAL_MODE_INPUT_ANALOG);
		adc_conf = &adc_conf_3;
		break;
	case 4:
		palSetPadMode(GPIOB, 0, PAL_MODE_INPUT_ANALOG);
		adc_conf = &adc_conf_4;
		break;
	default:
		return CH_FAILED;
		break;
	}

	adcStart(&ADC_DRIVER, NULL);

	node.advertise(ir_pub, conf->topic);

	/*
	 * Enable sensors power supply.
	 */
	palClearPad(IREN_GPIO, IREN);

	for (;;) {
		ProximityMsg *msgp;
		if (ir_pub.alloc(msgp)) {
			adcConvert(&ADC_DRIVER, adc_conf, &sample, 1);
			msgp->distance = sample;
			ir_pub.publish(*msgp);
		}
	r2p::Thread::sleep(r2p::Time::ms(500)); //TODO: Node::sleep()
	}

	return CH_SUCCESS;
}
