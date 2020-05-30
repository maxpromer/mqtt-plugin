#ifndef __MQTT_CPP__
#define __MQTT_CPP__

#include "mqtt.h"

static const char *TAG = "MQTT";

bool tcpConnected = false;
bool mqttConnected = false;
int *mqttSock = NULL;

char *mqttTopic = NULL;
uint8_t *mqttPayload = NULL;

bool waitCONNACKFlag = false;
bool waitPUBACKFlag = false;
bool waitSUBACKFlag = false;

struct CallbackNode {
	char *topic;
	SubscribeEventCallback cb;

	struct CallbackNode *next;
};

CallbackNode *callbackHead = NULL;
CallbackNode *callbackTail = NULL;

void CONNACKProcess(uint8_t c) {
	static uint8_t state = 0;
	if (state == 0) {
		state = 1;
	} else if (state == 1) {
		if (c == 0) { // check Connect Return Code is 0 ?
          ESP_LOGI(TAG, "MQTT Connected");
		  mqttConnected = true;
        } else {
          ESP_LOGE(TAG, "Connection Refused : %d", c);
		  mqttConnected = false;
        }
		if (waitCONNACKFlag) waitCONNACKFlag = false;
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

		if (waitPUBACKFlag) waitPUBACKFlag = false;
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
		if (waitSUBACKFlag) waitSUBACKFlag = false;
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
			state = 4;
		}
	} else if (state == 4) {
		MsgId = 0;
		MsgId = ((uint16_t)c) << 8;

		state = 5;
	} else if (state == 5) {
		MsgId |= c;
		
		payloadLen = dataLen - 2 - topicLen - 2;
		payload = (uint8_t*)malloc(payloadLen + 1);
		memset(payload, 0, payloadLen + 1);
		payloadIndex = 0;

		state = 6;
	} else if (state == 6) {
		payload[payloadIndex] = c;
		payloadIndex++;
		if (payloadLen == payloadIndex) {
			ESP_LOGI(TAG, "MQTT Receive %s: %s , QoS: %d", topic, payload, QoS);
			if (QoS == 1) {
				uint8_t bufferLen = 2 + 2; // Fixed header + Variable header
				uint8_t buffer[bufferLen];
				buffer[0] = 0x40;
				buffer[1] = 2;
				buffer[2] = (MsgId >> 8) & 0xFF;
				buffer[3] = MsgId & 0xFF;

				if (write(*mqttSock, buffer, bufferLen) < 0) {
					ESP_LOGE(TAG, "... socket send failed");
					close(*mqttSock);
					tcpConnected = false;
				}
				ESP_LOGI(TAG, "Send PUBACK message: %d", MsgId);

				mqttTopic = topic;
				mqttPayload = payload;

				CallbackNode *node = callbackHead;
				while(node != NULL) {
					if (strcmp(topic, node->topic) == 0) {
						node->cb();
					}
					node = node->next;
				}
			}
			state = 0;
		}
	}
}

uint8_t rx_buffer[200];

void mqttTask(void*){
	uint8_t state = 0;
	uint8_t MSG = 0x00;
	uint32_t DataLength = 0;

	while (1) {
		if (tcpConnected) {
			int len = recv(*mqttSock, rx_buffer, sizeof(rx_buffer), 0);

            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
				close(*mqttSock);
                tcpConnected = false;
				continue;
            }
            
			for (uint32_t inx=0;inx<len;inx++) {
				uint8_t c = rx_buffer[inx];
				ESP_LOGI(TAG, "Recv: 0x%02x", c);

				if (state == 0) {
					MSG = c;
					state = 1;
				} else if (state == 1) {
					DataLength = c;
					state = 2;
				} else if (state == 2) {
					if ((MSG&0xF0) == 0x20) { // CONNACK
						CONNACKProcess(c);
					} else if ((MSG&0xF0) == 0x40) { // PUBACK
						PUBACKProcess(c);
					} else if ((MSG&0xF0) == 0x90) { // SUBACK
						SUBACKProcess(c);
					} else if ((MSG&0xF0) == 0x30) { // PUBLISH
						PUBLISHProcess(c, MSG, DataLength);
					}
					DataLength--;
					if (DataLength == 0) {
						state = 0;
					}
				}
			}
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
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

	xTaskCreate(mqttTask, "mqttTask", 2048, NULL, 10, NULL);
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

void MQTT::process(Driver *drv)
{
}

void MQTT::connect(char *host, uint16_t port, char *clientId, char *username, char *password) {
	if (sock >= 0) {
		close(sock);
		sock = -1;
	}
	tcpConnected = false;

	struct addrinfo hints;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	struct addrinfo *res;
	struct in_addr *addr;

	char port_s[10];
	itoa(port, port_s, 10);
	ESP_LOGI(TAG, "Connect to %s:%s", host, port_s);
	int err = getaddrinfo(host, port_s, &hints, &res);

	if (err != 0 || res == NULL) {
		ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
		return;
	}

	addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
	ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

	sock = socket(res->ai_family, res->ai_socktype, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "... Failed to allocate socket.");
		freeaddrinfo(res);
		tcpConnected = false;
		return;
	}
	ESP_LOGI(TAG, "... allocated socket");

	if (::connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
		close(sock);
		freeaddrinfo(res);
		tcpConnected = false;
		return;
	}

	ESP_LOGI(TAG, "... connected");
	freeaddrinfo(res);

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
	buffer[11] = 10;		// Keep Alive LSB -> 10 s

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

	waitCONNACKFlag = true;
	ESP_LOGI(TAG, "Wait MQTT Respont");

	uint16_t i = 0;
	while(waitCONNACKFlag && (i < 5000) && tcpConnected) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		i += 10;
	}

	if (waitCONNACKFlag) {
		ESP_LOGE(TAG, "MQTT Connect fail.");
	} else {
		ESP_LOGE(TAG, "MQTT Connected.");
	}
	
}

bool MQTT::isConnected() {
	return tcpConnected && mqttConnected;
}

void MQTT::publish(char *topic, char *value, uint8_t QoS) {
	if (!isConnected()) return;

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

	if (QoS == 1) {
		ESP_LOGI(TAG, "Wait PUBACK");
		waitPUBACKFlag = true;

		uint16_t i = 0;
		while(waitPUBACKFlag && (i < 5000) && isConnected()) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			i += 10;
		}

		if (!waitPUBACKFlag) {
			ESP_LOGI(TAG, "Publish success !");
		} else {
			ESP_LOGE(TAG, "Publish Fail !");
		}
	}
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

void MQTT::subscribe(char *topic, SubscribeEventCallback cb, int maxQoS) {
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

	uint16_t i = 0;
	while(waitSUBACKFlag && (i < 5000) && isConnected()) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
		i += 10;
	}

	if (waitSUBACKFlag) {
		ESP_LOGE(TAG, "Subscribe Fail !");
		return;
	}

	ESP_LOGI(TAG, "Subscribe to %s , Max QoS: %d", topic, maxQoS);

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
