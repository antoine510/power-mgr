#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <mqtt/client.h>
#include <flatbuffers/flatbuffers.h>

class MQTTComms {
public:
	MQTTComms(const std::string& serviceName);
	~MQTTComms();

	void Subscribe(const std::string& topic, int qos, std::function<void(mqtt::const_message_ptr)>&& callback);
	bool Publish(mqtt::string_ref topic, const flatbuffers::FlatBufferBuilder& builder, int qos = 0, bool retain = false) {
		try {
			_client.publish(topic, builder.GetBufferPointer(), builder.GetSize(), qos, retain);
			return true;
		} catch(const mqtt::exception& e) {
			return false;
		}
	}
private:
	void ConsumerTask();

	std::vector<std::function<void(mqtt::const_message_ptr)>> _callbacks;

	std::thread _consumerThread;
	std::atomic<bool> _consumerActive{true};

	mqtt::client _client;
};
