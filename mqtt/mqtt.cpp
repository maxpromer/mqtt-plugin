#ifndef __MQTT_CPP__
#define __MQTT_CPP__

#include "mqtt.h"

static const char *TAG = "MQTT";

bool tcpConnected = false;
bool mqttConnected = false;
int *mqttSock = NULL;

char *mqttTopic = NULL;
uint8_t *mqttPayload = NULL;

volatile bool connectingFlag = false;
volatile bool publishFlag = false;
volatile bool subscribeFlag = false;

volatile bool waitCONNACKFlag = false;
volatile bool waitPUBACKFlag = false;
volatile bool waitSUBACKFlag = false;
volatile bool waitPUBRECFlag = false;
volatile bool waitPUBCOMPFlag = false;
volatile bool waitPUBRELFlag = false;

struct CallbackNode {
	char *topic;
	MQTTEventCallback cb;

	struct CallbackNode *next;
};

CallbackNode *callbackHead = NULL;
CallbackNode *callbackTail = NULL;

uint16_t trimString(char *str, uint16_t strLen, char c) {
	while(1) {
		if (str[strLen - 1] == c) {
			strLen--;
			if (strLen <= 0) break;
		} else {
			break;
		}
	}
	return strLen;
}

bool topicCheckMatch(char *topic1, char *topic2) {
	uint16_t topic1len = strlen(topic1);
	uint16_t topic2len = strlen(topic2);

	topic1len = trimString(topic1, topic1len, '/');
	topic2len = trimString(topic2, topic2len, '/');

	if (topic1[topic1len - 1] == '#') { // check multi level wildcard
		topic1len = trimString(topic1, topic1len - 1, '/');
		return strncmp(topic1, topic2, topic1len) == 0;
	} else {
		// Find single level wildcard
		int16_t plusPos = -1;
		for (uint16_t i=0;i<topic1len;i++) {
			if (topic1[i] == '+') {
				plusPos = i;
				break;
			}
		}

		if (plusPos >= 0) {
			bool frontMatch = strncmp(topic1, topic2, plusPos) == 0;

			uint8_t backLen = topic1len - plusPos - 1;
			uint16_t topic1BackOffset = topic1len - backLen;
			uint16_t topic2BackOffset = topic2len - backLen;

			bool backMatch = strncmp(&topic1[topic1BackOffset], &topic2[topic2BackOffset], backLen) == 0;

			return frontMatch && backMatch;
		} else {
			return strncmp(topic1, topic2, topic1len) == 0;
		}
	}
}

int64_t last_time_for_communication;

void updateTimer() { // Use for update last time for communication between server
	last_time_for_communication = esp_timer_get_time() / 1000;
}

void waitAllFlag() {
	/*
	while(connectingFlag || publishFlag || subscribeFlag || waitCONNACKFlag || waitPUBACKFlag || waitPUBRECFlag || waitPUBRELFlag || waitPUBCOMPFlag || waitSUBACKFlag) {
		ESP_LOGI(TAG, "connectingFlag: %d, publishFlag: %d, subscribeFlag: %d, waitCONNACKFlag: %d, waitPUBACKFlag: %d, waitPUBRECFlag: %d, waitPUBRELFlag: %d, waitPUBCOMPFlag: %d, waitSUBACKFlag: %d", connectingFlag, publishFlag, subscribeFlag, waitCONNACKFlag, waitPUBACKFlag, waitPUBRECFlag, waitPUBRELFlag, waitPUBCOMPFlag, waitSUBACKFlag);
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
	*/
}

void CONNACKProcess(uint8_t c, MQTT *mqtt) {
	static uint8_t state = 0;
	if (state == 0) {
		state = 1;
	} else if (state == 1) {
		if (waitCONNACKFlag) {
			waitCONNACKFlag = false;
		}
		if (c == 0) { // check Connect Return Code is 0 ?
          	ESP_LOGI(TAG, "MQTT Connected");
		  	mqttConnected = true;
		  	if (mqtt->onConnected_cb) {
				mqtt->onConnected_cb();
			}
        } else {
          	ESP_LOGE(TAG, "Connection Refused : %d", c);
		  	mqttConnected = false;
        }
		state = 0;
	}
}

void PUBACKProcess(uint8_t c) {
	static uint8_t state = 0;
	static uint16_t MsgId = 0;

	if (state == 0) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;
		state = 1;
	} else if (state == 1) {
		MsgId |= c;
		
		ESP_LOGI(TAG, "MQTT PUBACK MsgId: %d", MsgId);
		state = 0;

		if (waitPUBACKFlag) {
			waitPUBACKFlag = false;
		}
	}
}

void SUBACKProcess(uint8_t c) {
	static uint8_t state = 0;
	static uint16_t PacketId = 0;

	if (state == 0) {
		PacketId = ((uint16_t)c) << 8;
		state = 1;
	} else if (state == 1) {
		PacketId |= c;
		state = 2;
	} else if (state == 2) {
		uint8_t returnCode = c;
		ESP_LOGI(TAG, "Subscribe acknowledgement %d: %d", PacketId, returnCode);
		if (waitSUBACKFlag) {
			waitSUBACKFlag = false;
		}
		state = 0;
	}
}

void PUBLISHProcess(uint8_t c, uint8_t fixed_header, uint32_t data_len) {
	static uint8_t state = 0;
	static uint16_t MsgId = 0;
	static uint8_t QoS = 0;
	static uint32_t dataLen = 0;
	static char *topic = NULL;
	static uint16_t topicLen = 0;
	static uint16_t topicIndex = 0;
	static uint8_t *payload = NULL;
	static uint16_t payloadLen = 0;
	static uint16_t payloadIndex = 0;


    // ESP_LOGI(TAG, "PUBLISHProcess State: %d", state);
	if (state == 0) {
		dataLen = data_len;
		QoS = 0;
		QoS = (fixed_header >> 1) & 0b11;

		topicLen = 0;
		if (topic) {
			free(topic);
			topic = NULL;

		}
		topicIndex = 0;

		payloadLen = 0;
		if (payload) {
			free(payload);
			payload = NULL;
		}
		payloadIndex = 0;

		topicLen = ((uint16_t)c) << 8;
		state = 2;
	} else if (state == 2) {
		topicLen |= c;
		topic = (char*)malloc(topicLen + 1);
		memset(topic, 0, topicLen + 1);
		topicIndex = 0;

		state = 3;
	} else if (state == 3) {
		topic[topicIndex] = c;
		topicIndex++;
		if (topicLen == topicIndex) {
			payloadLen = dataLen - 2 - topicLen - (QoS == 0 ? 0 : 2); // QoS is 0, Not MsgId
			payload = (uint8_t*)malloc(payloadLen + 1);
			memset(payload, 0, payloadLen + 1);
			payloadIndex = 0;

			if (QoS == 0) {
				state = 6;
			} else {
				state = 4;
			}
		}
	} else if (state == 4) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;

		state = 5;
	} else if (state == 5) {
		MsgId |= c;

		state = 6;
	} else if (state == 6) {
		payload[payloadIndex] = c;
		payloadIndex++;
		if (payloadLen == payloadIndex) {
			ESP_LOGI(TAG, "MQTT Receive %s: %s , QoS: %d", topic, payload, QoS);
			if (QoS >= 1 && QoS <= 2) {
				uint8_t bufferLen = 2 + 2; // Fixed header + Variable header
				uint8_t buffer[bufferLen];
				buffer[0] = QoS == 1 ? 0x40 : 0x50; // if QoS1 send PUBACK, if QoS2 send PUBREC
				buffer[1] = 2;
				buffer[2] = (MsgId >> 8) & 0xFF;
				buffer[3] = MsgId & 0xFF;

				if (write(*mqttSock, buffer, bufferLen) < 0) {
					ESP_LOGE(TAG, "%s socket send failed", QoS == 1 ? "PUBACK" : "PUBREC");
					close(*mqttSock);
					tcpConnected = false;
				}
				ESP_LOGI(TAG, "Send %s message: %d", QoS == 1 ? "PUBACK" : "PUBREC", MsgId);
				if (QoS == 2) {
					ESP_LOGI(TAG, "Wait PUBREL");
					waitPUBRELFlag = true;
				}
				updateTimer(); 
			}
			mqttTopic = topic;
			mqttPayload = payload;

			CallbackNode *node = callbackHead;
			while(node != NULL) {
				// if (strcmp(topic, node->topic) == 0) {
				if (topicCheckMatch(node->topic, topic)) {
					if (node->cb) node->cb();
				}
				node = node->next;
			}

			state = 0;
		}
	}
}

void PUBRECProcess(uint8_t c) {
	static uint8_t state = 0;
	static uint16_t MsgId = 0;

	if (state == 0) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;

		state = 1;
	} else if (state == 1) {
		MsgId |= c;

		if (waitPUBRECFlag) {
			waitPUBRECFlag = false;
		}

		// Send PUBREL message
		uint8_t bufferLen = 2 + 2; // Fixed header + Variable header
		uint8_t buffer[bufferLen];
		buffer[0] = 0x60 | 0b0010;
		buffer[1] = 2;
		buffer[2] = (MsgId >> 8) & 0xFF;
		buffer[3] = MsgId & 0xFF;

		if (write(*mqttSock, buffer, bufferLen) < 0) {
			ESP_LOGE(TAG, "Send PUBREL socket send failed");
			close(*mqttSock);
			tcpConnected = false;
		}
		ESP_LOGI(TAG, "Send PUBREL message: %d and wait PUBCOMP", MsgId);
		waitPUBCOMPFlag = true;

		updateTimer(); 

		state = 0;
	}
}

void PUBCOMPProcess(uint8_t c) {
	static uint8_t state = 0;
	static uint16_t MsgId = 0;

	if (state == 0) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;

		state = 1;
	} else if (state == 1) {
		MsgId |= c;

		ESP_LOGI(TAG, "Receive PUBCOMP message: %d", MsgId);
		if (waitPUBCOMPFlag) {
			waitPUBCOMPFlag = false;
		}

		state = 0;
	}
}

void PUBRELProcess(uint8_t c) {
	static uint8_t state = 0;
	static uint16_t MsgId = 0;

	if (state == 0) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;

		state = 1;
	} else if (state == 1) {
		MsgId |= c;

		if (waitPUBRELFlag) {
			waitPUBRELFlag = false;
		}

		// Send PUBCOMP message
		uint8_t bufferLen = 2 + 2; // Fixed header + Variable header
		uint8_t buffer[bufferLen];
		buffer[0] = 0x70;
		buffer[1] = 2;
		buffer[2] = (MsgId >> 8) & 0xFF;
		buffer[3] = MsgId & 0xFF;

		if (write(*mqttSock, buffer, bufferLen) < 0) {
			ESP_LOGE(TAG, "Send PUBCOMP socket send failed");
			close(*mqttSock);
			tcpConnected = false;
		}
		ESP_LOGI(TAG, "Send PUBCOMP message: %d", MsgId);
		updateTimer(); 

		state = 0;
	}
}

uint8_t rx_buffer[200];

void mqttTask(void* p){
	MQTT *mqtt = (MQTT *) p;

	uint8_t state = 0;
	uint8_t MSG = 0x00;
	uint32_t DataLength = 0;

	last_time_for_communication = esp_timer_get_time() / 1000;
	
	int64_t timeOnSetFlag = -1;

	while (1) {
		if (tcpConnected) {
			int len = recv(*mqttSock, rx_buffer, sizeof(rx_buffer), MSG_DONTWAIT); // Non Block I/O
			// int len = 0;

            if (len < 0 && errno != 11) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
				close(*mqttSock);
                tcpConnected = false;
				continue;
            } else if (len < 0 && errno == 11) {
				len = 0;
			}
            
			for (uint32_t inx=0;inx<len;inx++) {
				uint8_t c = rx_buffer[inx];
				ESP_LOGI(TAG, "Recv: 0x%02x", c);

				if (state == 0) {
					MSG = c;
					state = 1;
				} else if (state == 1) {
					DataLength = c;
					if (DataLength > 0) {
						state = 2;
					} else {
						state = 0;
					}
				} else if (state == 2) {
					if ((MSG&0xF0) == 0x20) { // CONNACK
						CONNACKProcess(c, mqtt);
					} else if ((MSG&0xF0) == 0x40) { // PUBACK
						PUBACKProcess(c);
					} else if ((MSG&0xF0) == 0x90) { // SUBACK
						SUBACKProcess(c);
					} else if ((MSG&0xF0) == 0x30) { // PUBLISH
						PUBLISHProcess(c, MSG, DataLength);
					} else if ((MSG&0xF0) == 0x50) { // PUBREC
						PUBRECProcess(c);
					} else if ((MSG&0xF0) == 0x70) { // PUBCOMP
						PUBCOMPProcess(c);
					} else if ((MSG&0xF0) == 0x60) { // PUBREL
						PUBRELProcess(c);
					}
					DataLength--;
					if (DataLength == 0) {
						state = 0;
					}
				}
			}
/*
			if (len == 0) {
				if (((esp_timer_get_time() / 1000) - last_time_for_communication) > 5000) {
					ESP_LOGI(TAG, "Send PINGREQ");

					uint8_t buffer[2] = { 0xC0, 0 };

					if (write(*mqttSock, buffer, 2) < 0) {
						ESP_LOGE(TAG, "PINGREQ send failed: errno %d", errno);
						close(*mqttSock);
						tcpConnected = false;
					}
					updateTimer();
				}
			}*/

			if (waitCONNACKFlag || waitPUBACKFlag || waitPUBRECFlag || waitPUBCOMPFlag || waitPUBRELFlag || waitSUBACKFlag) {
				// ESP_LOGI(TAG, "Wait some flag. %lld", timeOnSetFlag);
				if (timeOnSetFlag == -1) {
					timeOnSetFlag = esp_timer_get_time() / 1000;
				} else {
					if (((esp_timer_get_time() / 1000) - timeOnSetFlag) > 3000) {
						close(*mqttSock);
						tcpConnected = false;

						if (waitCONNACKFlag) {
							ESP_LOGE(TAG, "MQTT Conncet fail.");
							waitCONNACKFlag = false;
						}

						if (waitPUBACKFlag) {
							ESP_LOGE(TAG, "MQTT Publish QoS 1 fail.");
							waitPUBACKFlag = false;
						}

						if (waitPUBRECFlag) {
							ESP_LOGE(TAG, "MQTT Publish QoS 2 part 1 fail.");
							waitPUBRECFlag = false;
						}

						if (waitPUBCOMPFlag) {
							ESP_LOGE(TAG, "MQTT Publish QoS 2 part 3 fail.");
							waitPUBCOMPFlag = false;
						}

						if (waitPUBRELFlag) {
							ESP_LOGE(TAG, "MQTT Receive Publish QoS 2 part 2 fail.");
							waitPUBRELFlag = false;
						}

						if (waitSUBACKFlag) {
							ESP_LOGE(TAG, "MQTT Subscribe fail.");
							waitSUBACKFlag = false;
						}

						timeOnSetFlag = -1;
					}
				}
			} else {
				timeOnSetFlag = -1;

				if (((esp_timer_get_time() / 1000) - last_time_for_communication) > 20000) {
					ESP_LOGI(TAG, "Send PINGREQ");

					uint8_t buffer[2] = { 0xC0, 0 };

					if (write(*mqttSock, buffer, 2) < 0) {
						ESP_LOGE(TAG, "PINGREQ send failed: errno %d", errno);
						close(*mqttSock);
						tcpConnected = false;
					}
					updateTimer();
				}
			}
		} else {
			mqtt->connect();
		}
		vTaskDelay(20 / portTICK_PERIOD_MS);
	}
}

MQTT::MQTT() {}

void MQTT::init(void) {
	esp_log_level_set("*", ESP_LOG_INFO);

	// clear error flag
	error = false;
	// set initialized flag
	initialized = true;

	mqttSock = &sock;

	xTaskCreate(mqttTask, "mqttTask", 2048, this, 10, NULL);
}

int MQTT::prop_count(void)
{
	return 0;
}

bool MQTT::prop_name(int index, char *name)
{
	// not supported
	return false;
}

bool MQTT::prop_unit(int index, char *unit)
{
	// not supported
	return false;
}

bool MQTT::prop_attr(int index, char *attr)
{
	// not supported
	return false;
}

bool MQTT::prop_read(int index, char *value)
{
	// not supported
	return false;
}

bool MQTT::prop_write(int index, char *value)
{
	// not supported
	return false;
}

void MQTT::process(Driver *drv) {
	wifiConnected = (uint32_t)((GPIO_REG_READ(GPIO_OUT_REG) >> 2) & 1U) == 0;
	
	gpio_set_level(GPIO_NUM_12, isConnected() ? 0 : 1);
}

void MQTT::config(char* host, uint16_t port, char *clientId, char *username, char *password) {
	this->host = host;
	this->port = port;
	this->clientId = clientId;
	this->username = username;
	this->password = password;
}

ip_addr_t addr;
bool bDNSFound = false;

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    ESP_LOGI("ota", "%sfound host ip errno: %d", ipaddr == NULL?"NOT ":"", errno);
	bDNSFound = true;
    if(ipaddr == NULL) return;

    addr = *ipaddr;
}

void MQTT::connect() {
	if (sock >= 0) {
		// shutdown(sock, 0);
		close(sock);
		sock = -1;
	}
	tcpConnected = false;

	if (strlen(host) == 0) {
		return;
	}

	connectingFlag = true;

	waitCONNACKFlag = false;
	waitPUBACKFlag = false;
	waitPUBCOMPFlag = false;
	waitPUBRECFlag = false;
	waitPUBRELFlag = false;
	waitSUBACKFlag = false;
/*
    if (true) {
		// Test connect to Google
		struct sockaddr_in dest_addr;
		dest_addr.sin_addr.s_addr = inet_addr("1.1.1.1");
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(80);

		int testSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (testSock < 0) {
            ESP_LOGE(TAG, "Test Connect : Unable to create socket: errno %d", errno);
			vTaskDelay(200 / portTICK_PERIOD_MS);
            return;
        }

        int err = ::connect(testSock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0) {
			close(testSock);
            ESP_LOGE(TAG, "Test Connect : Socket unable to connect: errno %d", errno);
			vTaskDelay(200 / portTICK_PERIOD_MS);
            return;
        }

		close(testSock);
	}*/
	if (!wifiConnected) {
		ESP_LOGE(TAG, "WiFi not connect");
		return;
	}
/*
	vTaskDelay(500 / portTICK_PERIOD_MS);
*/
/*
	// Set DNS Server
	ip_addr_t dnsserver;
	inet_pton(AF_INET, "8.8.8.8", &dnsserver);
	dns_setserver(0, &dnsserver);
	inet_pton(AF_INET, "8.8.4.4", &dnsserver);
	dns_setserver(1, &dnsserver);
*/
	
	bDNSFound = false;

	err_t err = dns_gethostbyname(host, &addr, &dns_found_cb, &addr);
	if (err == ESP_OK && addr.u_addr.ip4.addr) {
		ESP_LOGI(TAG, "OK, got ip without find");
	} else {
		ESP_LOGI(TAG, "Start find");
		int i=0;
		while( !bDNSFound ){
			vTaskDelay(100 / portTICK_PERIOD_MS);
			i += 100;
			if (i > 5000) break;
		}
		ESP_LOGI(TAG, "Exit find");
	}

	ESP_LOGI(TAG, "DNS found IP: %i.%i.%i.%i",
        ip4_addr1(&addr.u_addr.ip4),
        ip4_addr2(&addr.u_addr.ip4),
        ip4_addr3(&addr.u_addr.ip4),
        ip4_addr4(&addr.u_addr.ip4));

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "... Failed to allocate socket.");
		// freeaddrinfo(res);
		tcpConnected = false;
		return;
	}
	ESP_LOGI(TAG, "... allocated socket");

	struct sockaddr_in sock_info;
	memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    memcpy((void *)&sock_info.sin_addr.s_addr, (const void *)(&addr.u_addr.ip4), 4);
    sock_info.sin_port = htons(port);

	if (::connect(sock, (struct sockaddr *)&sock_info, sizeof(sock_info)) != 0) {
		ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
		close(sock);
		// freeaddrinfo(res);
		tcpConnected = false;
		return;
	}

	ESP_LOGI(TAG, "... connected");
	// freeaddrinfo(res);

	tcpConnected = true;

	/* -------- Connect to MQTT -------- */
	uint16_t bufferSize = 12; // Fix Heder + Variable header
	bufferSize += 2 + strlen(clientId);
	if (strlen(username) > 0) {
		bufferSize += 2 + strlen(username);
	}
	if (strlen(password) > 0) {
		bufferSize += 2 + strlen(password);
	}

	uint8_t buffer[bufferSize];
	memset(buffer, 0, sizeof buffer);

	// Fix Heder
	buffer[0] = 0x10;		// MSG Type -> CONNECT message
	buffer[1] = bufferSize - 2;	// Remaining Length -> Variable header + Payload

	// Variable header

	// -> Protocol Name
	buffer[2] = 0;			// Length MSB
	buffer[3] = 4;			// Length LSB
	buffer[4] = 'M';
	buffer[5] = 'Q';
	buffer[6] = 'T';
	buffer[7] = 'T';

	// -> Protocol Version
	buffer[8] = 4;			// Version -> 4

	// -> Connect Flags
	buffer[9] = 0b00000010; // Set Clean Session flag
	if (strlen(username) > 0) {
		buffer[9] |= 1<<7;
	}
	if (strlen(password) > 0) {
		buffer[9] |= 1<<6;
	}

	// -> Keep Alive timer
	buffer[10] = 0;			// Keep Alive MSB -> 0
	buffer[11] = 30;		// Keep Alive LSB -> 30 s

	// Payload

	// -> Client Identifier
	uint16_t ClientIdSize = strlen(clientId);
	buffer[12] = (ClientIdSize >> 8) & 0xFF;	// Length MSB -> 0
	buffer[13] = ClientIdSize & 0xFF;			// Length LSB -> 8

	uint16_t buffNext = 14;
	memcpy(&buffer[buffNext], clientId, strlen(clientId));
	buffNext += strlen(clientId);
	
	if (strlen(username) > 0) {
		uint16_t userLen = strlen(username);
		buffer[buffNext++] = userLen >> 8;
		buffer[buffNext++] = userLen & 0xFF;
		memcpy(&buffer[buffNext], username, userLen);
		buffNext += userLen;
	}

	if (strlen(password) > 0) {
		uint16_t passLen = strlen(password);
		buffer[buffNext++] = passLen >> 8;
		buffer[buffNext++] = passLen & 0xFF;
		memcpy(&buffer[buffNext], password, passLen);
		buffNext += passLen;
	}

	if (write(sock, buffer, buffNext) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		close(sock);
		tcpConnected = false;
		return;
	}
	ESP_LOGI(TAG, "... socket send success");
	updateTimer(); 


	waitCONNACKFlag = true;
	ESP_LOGI(TAG, "Wait MQTT Respont");

/*	uint16_t i = 0;
	while(waitCONNACKFlag && (i < 5000) && tcpConnected) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		i += 10;
	}

	if (waitCONNACKFlag) {
		ESP_LOGE(TAG, "MQTT Connect fail.");
		mqttConnected = false;
	} else {
		ESP_LOGE(TAG, "MQTT Connected.");
		if (onConnected_cb) {
			onConnected_cb();
		}
	}
	*/

	connectingFlag = false;
}

bool MQTT::isConnected() {
	return tcpConnected && mqttConnected;
}

void MQTT::onConnected(MQTTEventCallback cb) {
	onConnected_cb = cb;
}

void MQTT::publish(char *topic, char *value, uint8_t QoS) {
	if (!isConnected()) return;

	waitAllFlag();

	publishFlag = true;

	/* -------- Publish to MQTT -------- */
	uint16_t bufferSize = 2;				// Fix Heder
	bufferSize += 2 + strlen(topic) + 2;	// Variable header
	bufferSize += strlen(value);			// Payload

	uint8_t buffer[bufferSize];
	memset(buffer, 0, sizeof buffer);

	// Fixed header
  	buffer[0] = 0x30 | (QoS << 1) | 0b0001; // PUBLISH message ->  QoS level = 1,  RETAIN = 1
  	buffer[1] = bufferSize - 2; // Remaining Length -> 2 for Topic name length byte, 2 for Message Identifier byte
  
	// Variable header
	// -> Topic name
	uint16_t topicLen = strlen(topic);
	buffer[2] = (topicLen >> 8) & 0xFF;
  	buffer[3] = topicLen & 0xFF;

	uint16_t buffNext = 4;
  	memcpy(&buffer[buffNext], topic, topicLen);
	buffNext += topicLen;

	// -> Message Identifier
	buffer[buffNext++] = msgId >> 8;
  	buffer[buffNext++] = msgId & 0xFF;

	msgId++;

	// Payload
	uint16_t valueLen = strlen(value);
  	memcpy(&buffer[buffNext], value, valueLen);
	buffNext += valueLen;

	if (write(sock, buffer, buffNext) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		close(sock);
		tcpConnected = false;
		return;
	}
	
	ESP_LOGI(TAG, "... socket send success");
	updateTimer(); 


	if (QoS == 1) {
		ESP_LOGI(TAG, "Wait PUBACK");
		waitPUBACKFlag = true;
	} else if (QoS == 2) {
		ESP_LOGI(TAG, "Wait PUBREC");
		waitPUBRECFlag = true;
	}
	/*
		uint16_t i = 0;
		while(waitPUBACKFlag && (i < 5000) && isConnected()) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			i += 10;
		}

		if (!waitPUBACKFlag) {
			ESP_LOGI(TAG, "Publish success !");
		} else {
			ESP_LOGE(TAG, "Publish Fail !");
			mqttConnected = false;
		}
	*/

	publishFlag = false;
}

void MQTT::publish(char *topic, double value, uint8_t QoS) {
	char buff[20];
	sprintf(buff, "%f", value);
	this->publish(topic, buff, QoS);
}

void MQTT::publish(char *topic, int value, uint8_t QoS) {
	char buff[20];
	sprintf(buff, "%d", value);
	this->publish(topic, buff, QoS);
}

void MQTT::publish(char *topic, bool value, uint8_t QoS) {
	char buff[2] = {value ? '1' : '0', 0};
	this->publish(topic, buff, QoS);
}

void MQTT::subscribe(char *topic, MQTTEventCallback cb, int maxQoS) {
	if (!isConnected()) return;

    waitAllFlag();

	subscribeFlag = true;
	
	uint16_t bufferSize = 2;				// Fix Heder
	bufferSize += 2;						// Variable header
	bufferSize += 2 + strlen(topic) + 1;	// Payload

	uint8_t buffer[bufferSize];
	memset(buffer, 0, sizeof buffer);
	
	// Fix Header
	buffer[0] = 0x80 | 0b0010; // SUBSCRIBE
    buffer[1] = bufferSize - 2; // Remaining Length

	// Variable header
	buffer[2] = msgId >> 8; // -> Message ID MSB
	buffer[3] = msgId & 0xFF; // -> Message ID LSB

	msgId++;

	// Payload
	uint16_t topicLen = strlen(topic);
	buffer[4] = topicLen >> 8;
	buffer[5] = topicLen & 0xFF;

	uint16_t buffNext = 6;
	memcpy(&buffer[buffNext], topic, topicLen);
	buffNext += topicLen;

	buffer[buffNext++] = maxQoS; // QoS

	if (write(sock, buffer, buffNext) < 0) {
		ESP_LOGE(TAG, "... socket send failed");
		close(sock);
		tcpConnected = false;
		return;
	}

	
	ESP_LOGI(TAG, "Wait SUBACK");
	waitSUBACKFlag = true;

/*
	uint16_t i = 0;
	while(waitSUBACKFlag && (i < 5000) && isConnected()) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		i += 10;
	}

	if (waitSUBACKFlag) {
		ESP_LOGE(TAG, "Subscribe Fail !");
		mqttConnected = false;
		return;
	}
    */

	ESP_LOGI(TAG, "Subscribe %d to %s , Max QoS: %d", msgId - 1, topic, maxQoS);
	updateTimer(); 

	// Add Callback
	CallbackNode *node = new CallbackNode;
	node->topic = topic;
	node->cb = cb;
	node->next = NULL;

	if (callbackHead == NULL && callbackTail == NULL) {
		callbackHead = node;
		callbackTail = node;
	} else {
		callbackTail->next = node;
		callbackTail = node;
	}

	subscribeFlag = false;
}

char* MQTT::getTopic() {
	return mqttTopic;
}

double MQTT::getNumber() {
	return atof((char*)mqttPayload);
}

char* MQTT::getText() {
	return (char*)mqttPayload;
}

#endif
