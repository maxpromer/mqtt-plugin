/*
Blockly.Blocks['mqtt_connect'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_connect",
			"message0": "MQTT Connect %1 Host %2 Port %3 Client Id %4 Username %5 Password %6",
			"args0": [
				{
					"type": "input_dummy"
				},
				{
					"type": "input_value",
					"name": "host",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "port",
					"check": "Number",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "client_id",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "username",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "password",
					"check": "String",
					"align": "RIGHT"
				}
			],
			"previousStatement": null,
			"nextStatement": null,
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};
*/

Blockly.Blocks['mqtt_config'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_config",
			"message0": "MQTT Config %1 Host %2 Port %3 Client Id %4 Username %5 Password %6",
			"args0": [
				{
					"type": "input_dummy"
				},
				{
					"type": "input_value",
					"name": "host",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "port",
					"check": "Number",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "client_id",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "username",
					"check": "String",
					"align": "RIGHT"
				},
				{
					"type": "input_value",
					"name": "password",
					"check": "String",
					"align": "RIGHT"
				}
			],
			"previousStatement": null,
			"nextStatement": null,
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_on_connected'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_on_connected",
			"message0": "MQTT on Connected %1 %2",
			"args0": [
				{
					"type": "input_dummy"
				},
				{
					"type": "input_statement",
					"name": "callback"
				}
			],
			"previousStatement": null,
			"nextStatement": null,
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_is_connect'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_is_connect",
			"message0": "MQTT is connected ?",
			"output": [
				"Number",
				"Boolean"
			],
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_publish'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_publish",
			"message0": "MQTT Publish  topic : %1   data: %2",
			"args0": [
				{
					"type": "field_input",
					"name": "topic",
					"text": ""
				},
				{
					"type": "input_value",
					"name": "value",
					"check": [
						"Boolean",
						"Number",
						"String"
					],
					"align": "RIGHT"
				}
			],
			"inputsInline": true,
			"previousStatement": null,
			"nextStatement": null,
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_subscribe'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_subscribe",
			"message0": "MQTT Subscribe   topic : %1 on receive %2 %3",
			"args0": [
				{
					"type": "field_input",
					"name": "topic",
					"text": ""
				},
				{
					"type": "input_dummy"
				},
				{
					"type": "input_statement",
					"name": "callback"
				}
			],
			"previousStatement": null,
			"nextStatement": null,
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_get_topic'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_get_text",
			"message0": "MQTT get topic",
			"output": "String",
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_get_number'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_get_number",
			"message0": "MQTT get payload number",
			"output": [
				"Number",
				"Boolean"
			],
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};

Blockly.Blocks['mqtt_get_text'] = {
	init: function () {
		this.jsonInit({
			"type": "mqtt_get_text",
			"message0": "MQTT get payload text",
			"output": "String",
			"colour": 180,
			"tooltip": "",
			"helpUrl": ""
		});
	}
};
