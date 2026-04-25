#include "pch.h"
#include "Utils.h"

namespace tc::utils
{
	std::string generateUUID()
	{
		static boost::uuids::random_generator gen;
		return boost::uuids::to_string(gen());
	}
}
