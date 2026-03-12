#pragma once
#include <cstring>
#include <cstdint>
#include <vector>

namespace net
{
	template<class T>
	struct MessageHeader
	{
		T id{};
		uint32_t size = 0;
	};

	template<class T>
	struct Message
	{
		MessageHeader<T> header{};
		std::vector<uint8_t> body;

		size_t size() const { return body.size(); }

		template<typename DataType>
			requires std::is_trivially_copyable_v<DataType>
		Message<T>& operator<<(const DataType& data)
		{
			size_t i = body.size();
			body.resize(i + sizeof(DataType));
			std::memcpy(body.data() + i, &data, sizeof(DataType));
			header.size = static_cast<uint32_t>(body.size());
			return *this;
		}

		template<typename DataType>
			requires std::is_trivially_copyable_v<DataType>
		Message<T>& operator>>(DataType& data)
		{
			size_t i = body.size() - sizeof(DataType);
			std::memcpy(&data, body.data() + i, sizeof(DataType));
			body.resize(i);
			header.size = static_cast<uint32_t>(body.size());
			return *this;
		}

		TODO
		template<typename T>
		Message<T>& operator<<(const std::string& str)
		{
			uint32_t len = str.size();
			size_t i = body.size();
			body.resize(i + len);
			std::memcpy(body.data() + i, str.data(), len);
			header.size = static_cast<uint32_t>(size());
			return *this;
		}

		template<typename T>
		Message<T>& operator>>(std::string& str)
		{
			uint32_t len;
			*this >> len;                        // read length first

			str.resize(len);
			std::memcpy(str.data(), body.data() + (body.size() - len), len);
			body.resize(body.size() - len);
			header.size = static_cast<uint32_t>(body.size());
			return *this;
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
