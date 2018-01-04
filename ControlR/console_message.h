#pragma once

namespace Console {

	typedef enum {
		CONSOLE_MESSAGE = 0,
		CONSOLE_ERROR = 1,
		PROMPT = 2
	}
	MessageType;

	class Message {
	public:
		MessageType type_;
		std::string message_;

	public:
		Message(const std::string &message = "", MessageType type = MessageType::CONSOLE_MESSAGE)
			: message_(message)
			, type_(type) 
		{}

		Message(const Message &rhs) {
			message_ = rhs.message_;
			type_ = rhs.type_;
		}

	public:
		size_t length() { return message_.length(); }

	};

	class Prompt : public Message {
	public:
		Prompt(const std::string &message) : Message(message, MessageType::PROMPT) {}
	};

	class Error: public Message {
	public:
		Error(const std::string &message) : Message(message, MessageType::CONSOLE_ERROR) {}
	};

}