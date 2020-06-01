/*
Blockly.JavaScript['mqtt_connect'] = function (block) {
	var value_host = Blockly.JavaScript.valueToCode(block, 'host', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';
	var value_port = Blockly.JavaScript.valueToCode(block, 'port', Blockly.JavaScript.ORDER_ATOMIC) || '1883';
	var value_client_id = Blockly.JavaScript.valueToCode(block, 'client_id', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)"KidBright32"';
	var value_username = Blockly.JavaScript.valueToCode(block, 'username', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';
	var value_password = Blockly.JavaScript.valueToCode(block, 'password', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';

	var code = `DEV_IO.MQTT().connect(${value_host}, ${value_port}, ${value_client_id}, ${value_username}, ${value_password});\n`;
	return code;
};*/

Blockly.JavaScript['mqtt_config'] = function (block) {
	var value_host = Blockly.JavaScript.valueToCode(block, 'host', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';
	var value_port = Blockly.JavaScript.valueToCode(block, 'port', Blockly.JavaScript.ORDER_ATOMIC) || '1883';
	var value_client_id = Blockly.JavaScript.valueToCode(block, 'client_id', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)"KidBright32"';
	var value_username = Blockly.JavaScript.valueToCode(block, 'username', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';
	var value_password = Blockly.JavaScript.valueToCode(block, 'password', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';

	var code = `DEV_IO.MQTT().config(${value_host}, ${value_port}, ${value_client_id}, ${value_username}, ${value_password});\n`;
	return code;
};

Blockly.JavaScript['mqtt_on_connected'] = function (block) {
	var statements_callback = Blockly.JavaScript.statementToCode(block, 'callback');

	var code = `DEV_IO.MQTT().onConnected([]() { ${statements_callback} });\n`;
	return code;
};

Blockly.JavaScript['mqtt_is_connect'] = function (block) {
	var code = 'DEV_IO.MQTT().isConnected()';
	return [code, Blockly.JavaScript.ORDER_NONE];
};

Blockly.JavaScript['mqtt_publish'] = function (block) {
	var text_topic = block.getFieldValue('topic');
	var value_value = Blockly.JavaScript.valueToCode(block, 'value', Blockly.JavaScript.ORDER_ATOMIC) || '(char *)""';

	var code = `DEV_IO.MQTT().publish("${text_topic}", ${value_value});\n`;
	return code;
};

Blockly.JavaScript['mqtt_subscribe'] = function (block) {
	var text_topic = block.getFieldValue('topic');
	var statements_callback = Blockly.JavaScript.statementToCode(block, 'callback');
	// TODO: Assemble JavaScript into code variable.
	var code = `DEV_IO.MQTT().subscribe("${text_topic}", []() { ${statements_callback} });\n`;
	return code;
};

Blockly.JavaScript['mqtt_get_topic'] = function (block) {
	var code = 'DEV_IO.MQTT().getTopic()';
	return [code, Blockly.JavaScript.ORDER_NONE];
};

Blockly.JavaScript['mqtt_get_number'] = function (block) {
	var code = 'DEV_IO.MQTT().getNumber()';
	return [code, Blockly.JavaScript.ORDER_NONE];
};

Blockly.JavaScript['mqtt_get_text'] = function (block) {
	var code = 'DEV_IO.MQTT().getText()';
	return [code, Blockly.JavaScript.ORDER_NONE];
};
