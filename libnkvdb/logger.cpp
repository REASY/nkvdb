#include "logger.h"

#include <iostream>

namespace utils
{
	Logger* Logger::get() {
		static Logger instance;
		return &instance;
	}

	std::ostream& Logger::GetStream() {
		return std::cerr;
	}
}