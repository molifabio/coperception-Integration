#ifndef COMMON_CONFIG_H_
#define COMMON_CONFIG_H_

#include <algorithm>
#include <map>
#include <string>

namespace konro {

class Config {
public:
	static const Config &get(const std::string &filename="") {
		static Config c(filename);
		return c;
	}

	template<typename T>
	T read(const std::string &section, const std::string &key) const;

private:
	Config(const std::string &);
	std::map<std::pair<std::string,std::string>, std::string> internal;
};

} // namespace konro

#endif
