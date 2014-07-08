#include "ch.h"
#include "hal.h"
#include "ir.h"
#include "sonar.h"

#include <r2p/Middleware.hpp>
#include <r2p/node/led.hpp>


#ifndef R2P_MODULE_NAME
#define R2P_MODULE_NAME "IR"
#endif

static WORKING_AREA(wa_info, 1024);
static r2p::RTCANTransport rtcantra(RTCAND1);

RTCANConfig rtcan_config = { 1000000, 100, 60 };

r2p::Middleware r2p::Middleware::instance(R2P_MODULE_NAME, "BOOT_"R2P_MODULE_NAME);

/*
 * Application entry point.
 */
extern "C" {
int main(void) {

	halInit();
	chSysInit();

	r2p::Middleware::instance.initialize(wa_info, sizeof(wa_info), r2p::Thread::LOWEST);
	rtcantra.initialize(rtcan_config);
	r2p::Middleware::instance.start();

	/* Led 2 subscriber. */
	r2p::ledsub_conf ledsub_conf = { "led2" };
	r2p::Thread::create_heap(NULL, THD_WA_SIZE(1024), NORMALPRIO + 1, r2p::ledsub_node, (void *) &ledsub_conf,
			"led2sub");

	/* Led 3 subscriber. */
	ledsub_conf.topic = "led3";
	r2p::Thread::create_heap(NULL, THD_WA_SIZE(1024), NORMALPRIO + 1, r2p::ledsub_node, (void *) &ledsub_conf,
			"led3sub");

	/* IR publisher. */
	irpub_conf irpub_conf = { "ir", 2};
	r2p::Thread::create_heap(NULL, THD_WA_SIZE(1024), NORMALPRIO + 1, irpub_node, (void *) &irpub_conf,
			"irpub");

	/* Sonar publisher. */
	sonarpub_conf sonarpub_conf = { "sonar", 1};
	r2p::Thread::create_heap(NULL, THD_WA_SIZE(1024), NORMALPRIO + 1, sonarpub_node, (void *) &sonarpub_conf,
			"sonarpub");

	r2p::Thread::set_priority(r2p::Thread::NORMAL);
	for (;;) {
		palTogglePad(LED1_GPIO, LED1);
		r2p::Thread::sleep(r2p::Time::ms(500));
	}
	return CH_SUCCESS;
}
}
