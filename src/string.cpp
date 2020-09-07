#include <cctype> // for tolower and toupper
#include <algorithm> // for transform
#include <libgen.h> // for dirname and basename

#if defined ARCH_WIN
	#include <windows.h> // for MultiByteToWideChar
#endif

#include <string.hpp>


namespace rack {
namespace string {


std::string f(const char* format, ...) {
	va_list args;
	va_start(args, format);
	// Compute size of required buffer
	int size = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (size < 0)
		return "";
	// Create buffer
	std::string s;
	s.resize(size);
	va_start(args, format);
	vsnprintf(&s[0], size + 1, format, args);
	va_end(args);
	return s;
}


std::string lowercase(const std::string& s) {
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) {
		return std::tolower(c);
	});
	return r;
}


std::string uppercase(const std::string& s) {
	std::string r = s;
	std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) {
		return std::toupper(c);
	});
	return r;
}


std::string trim(const std::string& s) {
	const std::string whitespace = " \n\r\t";
	size_t first = s.find_first_not_of(whitespace);
	if (first == std::string::npos)
		return "";
	size_t last = s.find_last_not_of(whitespace);
	if (last == std::string::npos)
		return "";
	return s.substr(first, last - first + 1);
}


std::string ellipsize(const std::string& s, size_t len) {
	if (s.size() <= len)
		return s;
	else
		return s.substr(0, len - 3) + "...";
}


std::string ellipsizePrefix(const std::string& s, size_t len) {
	if (s.size() <= len)
		return s;
	else
		return "..." + s.substr(s.size() - (len - 3));
}


bool startsWith(const std::string& str, const std::string& prefix) {
	return str.substr(0, prefix.size()) == prefix;
}


bool endsWith(const std::string& str, const std::string& suffix) {
	return str.substr(str.size() - suffix.size(), suffix.size()) == suffix;
}


float fuzzyScore(const std::string& s, const std::string& query) {
	size_t pos = s.find(query);
	if (pos == std::string::npos)
		return 0.f;

	return (float)(query.size() + 1) / (s.size() + 1);
}


std::string toBase64(const uint8_t* data, size_t dataLen) {
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	size_t numBlocks = (dataLen + 2) / 3;
	size_t strLen = numBlocks * 4;
	std::string str;
	str.reserve(strLen);

	for (size_t b = 0; b < numBlocks; b++) {
		// Encode block
		uint32_t block = 0;
		int i;
		for (i = 0; i < 3 && 3 * b + i < dataLen; i++) {
			block |= uint32_t(data[3 * b + i]) << (8 * (2 - i));
		}

		// Decode block
		str += alphabet[(block >> 18) & 0x3f];
		str += alphabet[(block >> 12) & 0x3f];
		str += (i > 1) ? alphabet[(block >> 6) & 0x3f] : '=';
		str += (i > 2) ? alphabet[(block >> 0) & 0x3f] : '=';
	}
	return str;
}


std::string toBase64(const std::vector<uint8_t>& data) {
	return toBase64(data.data(), data.size());
}


std::vector<uint8_t> fromBase64(const std::string& str) {
	std::vector<uint8_t> data;
	uint32_t block = 0;
	int i = 0;
	int padding = 0;

	for (char c : str) {
		uint8_t d = 0;

		if ('A' <= c && c <= 'Z') {
			d = c - 'A';
		}
		else if ('a' <= c && c <= 'z') {
			d = c - 'a' + 26;
		}
		else if ('0' <= c && c <= '9') {
			d = c - '0' + 52;
		}
		else if (c == '+') {
			d = 62;
		}
		else if (c == '/') {
			d = 63;
		}
		else if (c == '=') {
			padding++;
		}
		else {
			// Ignore whitespace and non-base64 characters
			continue;
		}

		block |= uint32_t(d) << (6 * (3 - i));
		i++;

		if (i >= 4) {
			// Decode block
			data.push_back((block >> (8 * (2 - 0))) & 0xff);
			if (padding < 2)
				data.push_back((block >> (8 * (2 - 1))) & 0xff);
			if (padding < 1)
				data.push_back((block >> (8 * (2 - 2))) & 0xff);
			// Reset block
			block = 0;
			i = 0;
			padding = 0;
		}
	}
	return data;
}


bool CaseInsensitiveCompare::operator()(const std::string& a, const std::string& b) const {
	if (a.size() != b.size())
		return false;
	auto f = [](unsigned char a, unsigned char b) {
		return std::tolower(a) == std::tolower(b);
	};
	return std::equal(a.begin(), a.end(), b.begin(), f);
}


#if defined ARCH_WIN
std::string U16toU8(const std::wstring& w) {
	if (w.empty())
		return "";
	// Compute length of output buffer
	int len = WideCharToMultiByte(CP_UTF8, 0, &w[0], w.size(), NULL, 0, NULL, NULL);
	assert(len > 0);
	std::string s;
	// Allocate enough space for null character
	s.resize(len);
	len = WideCharToMultiByte(CP_UTF8, 0, &w[0], w.size(), &s[0], len, 0, 0);
	assert(len > 0);
	return s;
}


std::wstring U8toU16(const std::string& s) {
	if (s.empty())
		return L"";
	// Compute length of output buffer
	int len = MultiByteToWideChar(CP_UTF8, 0, &s[0], s.size(), NULL, 0);
	assert(len > 0);
	std::wstring w;
	// Allocate enough space for null character
	w.resize(len);
	len = MultiByteToWideChar(CP_UTF8, 0, &s[0], s.size(), &w[0], len);
	assert(len > 0);
	return w;
}
#endif

} // namespace string
} // namespace rack
