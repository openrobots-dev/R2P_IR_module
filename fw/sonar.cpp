#include "ch.h"
#include "hal.h"
#include "sonar.h"

#include <r2p/Middleware.hpp>


static ICUConfig icu_conf = {
  ICU_INPUT_ACTIVE_HIGH,
  1000000,                                    /* 1MHz ICU clock frequency.   */
  NULL,
  NULL,
  NULL,
  ICU_CHANNEL_2,
  0
};


msg_t sonarpub_node(void * arg) {
	sonarpub_conf * conf = reinterpret_cast<sonarpub_conf *>(arg);
	r2p::Node node("sonarpub");
	r2p::Publisher<ProximityMsg> sonar_pub;
	icucnt_t sample;

	if (conf == NULL) return CH_FAILED;

	switch (conf->channel) {
	case 1:
		palSetPadMode(GPIOA, 1, PAL_MODE_INPUT);
		break;
	default:
		return CH_FAILED;
		break;
	}

	icuStart(&ICU_DRIVER, &icu_conf);
	icuEnable(&ICU_DRIVER);

	node.advertise(sonar_pub, conf->topic);

	/*
	 * Enable sensors power supply.
	 */
	palClearPad(IREN_GPIO, IREN);

	for (;;) {
		ProximityMsg *msgp;
		if (sonar_pub.alloc(msgp)) {
			sample = icuGetWidth(&ICU_DRIVER);
			msgp->distance = sample;
			sonar_pub.publish(*msgp);
		}
	r2p::Thread::sleep(r2p::Time::ms(500)); //TODO: Node::sleep()
	}

	return CH_SUCCESS;
}
