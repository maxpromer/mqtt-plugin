#ifndef __MQTT_H__
#define __MQTT_H__

#include "driver.h"
#include "device.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "kidbright32.h"

typedef void(*SubscribeEventCallback)();

static bool tcpConnected = false;
static bool mqttConnected = false;
static int *mqttSock = NULL;

static char *mqttTopic = NULL;
static uint8_t *mqttPayload = NULL;

class MQTT : public Device {
	private:
		int sock = -1;
		uint16_t msgId = 0;

	public:
		// constructor
		MQTT();
		// override
		void init(void);
		void process(Driver *drv);
		int prop_count(void);
		bool prop_name(int index, char *name);
		bool prop_unit(int index, char *unit);
		bool prop_attr(int index, char *attr);
		bool prop_read(int index, char *value);
		bool prop_write(int index, char *value);
		
		// method
		void connect(char* host, uint16_t port, char *clientId, char *username, char *password) ;
		bool isConnected() ;

		void publish(char *topic, char *value, uint8_t QoS = 1) ; 
		void publish(char *topic, double value, uint8_t QoS = 1) ;
		void publish(char *topic, int value, uint8_t QoS = 1) ; 
		void publish(char *topic, bool value, uint8_t QoS = 1) ; 

		void subscribe(char *topic, SubscribeEventCallback cb, int maxQoS = 2) ;

		char *getTopic()

};

#endif
