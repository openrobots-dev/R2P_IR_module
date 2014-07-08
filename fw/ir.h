#ifndef _IR_H_
#define _IR_H_

#include <r2p/Middleware.hpp>

/*
 * Proximity measurement message definition.
 */
struct ProximityMsg: public r2p::Message {
	float distance;
}R2P_PACKED;

/*
 * IR publisher node configuration.
 */
struct irpub_conf {
	const char * topic;
	uint8_t channel;
};

msg_t irpub_node(void * arg);

#endif /* _IR_H_ */
