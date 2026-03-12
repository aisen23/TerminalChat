#pragma once

namespace net
{
	template<class T>
	struct MessageHeader {
		T id{};
		uint32_t size = 0;
	};

	template<class T>
	struct Message {
		MessageHeader<T> header{};
		std::vector<uint8_t> body;

		size_t size() const { return body.size(); }

		template<typename DataType>
			requires std::is_trivially_copyable_v<DataType>
		Message<T>& operator<<(Message<T>& msg, const DataType& data)
		{
			size_t i = msg.body.size();
			msg.body.resize(i + sizeof(DataType));
			std::memcpy(msg.body.data() + i, &data, sizeof(DataType));
			msg.header.size = static_cast<uint32_t>(msg.body.size());
			return msg;
		}

		template<typename DataType>
			requires std::is_trivially_copyable_v<DataType>
		Message<T>& operator>>(Message<T>& msg, DataType& data)
		{
			size_t i = msg.body.size() - sizeof(DataType);
			std::memcpy(&data, msg.body.data() + i, sizeof(DataType));
			msg.body.resize(i);
			msg.header.size = static_cast<uint32_t>(msg.body.size());
			return msg;
		}
	};

	template <typename T>
	class Connection;

	template <typename T>
	struct OwnedMessage
	{
		std::shared_ptr<Connection<T>> remote{};
		Message<T> msg;

		std::ostream& operator<<(std::ostream& os, const OwnedMessage<T>& msg)
		{
			os << msg.msg;
			return os;
		}
	};
}
