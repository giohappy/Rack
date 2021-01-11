#include <common.hpp>
#include <string.hpp>


namespace rack {


const std::string APP_NAME = "VCV Rack";
const std::string APP_VERSION = TOSTRING(VERSION);
#if defined ARCH_WIN
	const std::string APP_ARCH = "win";
#elif ARCH_MAC
	const std::string APP_ARCH = "mac";
#elif defined ARCH_LIN
	const std::string APP_ARCH = "lin";
#endif

const std::string ABI_VERSION = "2";

const std::string API_URL = "https://api.vcvrack.com";
const std::string API_VERSION = "2";


Exception::Exception(const char* format, ...) {
	va_list args;
	va_start(args, format);
	msg = string::fV(format, args);
	va_end(args);
}


} // namespace rack


#if defined ARCH_WIN
#include <windows.h>

FILE* fopen_u8(const char* filename, const char* mode) {
	return _wfopen(rack::string::U8toU16(filename).c_str(), rack::string::U8toU16(mode).c_str());
}


#endif
