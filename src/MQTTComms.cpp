#include "MQTTComms.hpp"

MQTTComms::MQTTComms(const std::string& serviceName) : _client("localhost", serviceName, mqtt::create_options(MQTTVERSION_5)) {
	auto connOpts = mqtt::connect_options_builder()
		.mqtt_version(MQTTVERSION_5)
		.automatic_reconnect(std::chrono::seconds(2), std::chrono::seconds(30))
		.finalize();

	_client.connect(connOpts);

	_consumerActive.store(true);
	_consumerThread = std::thread(&MQTTComms::ConsumerTask, this);
}

MQTTComms::~MQTTComms() {
	_consumerActive.store(false);
	if(_consumerThread.joinable()) _consumerThread.join();
}

void MQTTComms::Subscribe(const std::string& topic, int qos, std::function<void(mqtt::const_message_ptr)>&& callback) {
	mqtt::properties props { { mqtt::property::SUBSCRIPTION_IDENTIFIER, (int32_t)_callbacks.size() + 1 } };
	_callbacks.emplace_back(std::move(callback));
	_client.subscribe(topic, qos, mqtt::subscribe_options(), props);
}

void MQTTComms::ConsumerTask() {
	while(_consumerActive.load(std::memory_order_relaxed)) {
		mqtt::const_message_ptr msg;
		_client.try_consume_message_for(&msg, std::chrono::seconds(1));
		if(!msg) continue;

		_callbacks[mqtt::get<uint8_t>(msg->get_properties(), mqtt::property::SUBSCRIPTION_IDENTIFIER) - 1](msg);
	}
}
