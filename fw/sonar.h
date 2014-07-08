#ifndef _SONAR_H_
#define _SONAR_H_

#include <r2p/Middleware.hpp>
#include "ir.h"

/*
 * Sonar publisher node configuration.
 */
struct sonarpub_conf {
	const char * topic;
	uint8_t channel;
};

msg_t sonarpub_node(void * arg);

#endif /* _IR_H_ */
