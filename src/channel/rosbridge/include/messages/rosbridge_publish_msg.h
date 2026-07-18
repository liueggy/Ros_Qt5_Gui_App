#pragma once

#include <iostream>

#include "messages/rosbridge_msg.h"

class ROSBridgePublishMsg : public ROSBridgeMsg {
private:
	// Declared before msg_json_ so its allocator is destroyed after the value.
	rapidjson::Document owned_msg_storage_;

public:
	ROSBridgePublishMsg() : ROSBridgeMsg() {}

	ROSBridgePublishMsg(bool init_opcode) : ROSBridgeMsg()
	{
		if (init_opcode)
			op_ = ROSBridgeMsg::PUBLISH;
	}

	virtual ~ROSBridgePublishMsg() = default;

	// This method parses the "topic" and "msg" fields from
	// incoming publish messages into this class
	bool FromJSON(rapidjson::Document &data) {
		if (!ROSBridgeMsg::FromJSON(data))
			return false;
		if (op_ != ROSBridgeMsg::PUBLISH) return false;

		if (!data.HasMember("topic") || !data["topic"].IsString() ||
		    data["topic"].GetStringLength() == 0 ||
		    data["topic"].GetStringLength() > rosbridge2cpp::validation::kMaxTopicLength) {
			std::cerr << "[ROSBridgePublishMsg] Received 'publish' message without 'topic' field." << std::endl; // TODO: use generic logging
			return false;
		}

		topic_.assign(data["topic"].GetString(), data["topic"].GetStringLength());

		if (!data.HasMember("msg") || !data["msg"].IsObject()) {
			std::cerr << "[ROSBridgePublishMsg] Received 'publish' message without 'msg' field." << std::endl;
			return false;
		}

		// Incoming envelopes stay alive for the duration of callback dispatch.
		// Move the root value to avoid copying large maps and camera payloads.
		msg_json_ = data["msg"];

		return true;
	}

	void SetMessage(const rapidjson::Value& message)
	{
		msg_json_.CopyFrom(message, owned_msg_storage_.GetAllocator());
	}

	rapidjson::Document ToJSON(rapidjson::Document::AllocatorType& /*alloc*/)
	{
		rapidjson::Document d(rapidjson::kObjectType);
		auto& alloc = d.GetAllocator();
		d.AddMember("op", getOpCodeString(), alloc);

		add_if_value_changed(d, alloc, "id", id_);
		add_if_value_changed(d, alloc, "topic", topic_);
		add_if_value_changed(d, alloc, "type", type_);


		d.AddMember("latch", latch_, alloc);

		if (!msg_json_.IsNull()) {
			rapidjson::Value payload;
			payload.CopyFrom(msg_json_, alloc);
			d.AddMember("msg", payload, alloc);
		}

		return d;
	}

	std::string topic_;
	std::string type_;
	// std::string compression_;
	// std::string throttle_rate_;
	// std::string queue_length_;
	bool latch_ = false;

	// The json data in the different wire-level representations
	rapidjson::Value msg_json_;

};
