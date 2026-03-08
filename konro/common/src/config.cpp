#include "config.h"

#include "constants.h"
#include "ini.h"

#include <iostream> // Fallback in case we cannot open the config file (we cannot still use log at this point)
#include <stdexcept>

namespace konro {

Config::Config(const std::string& filename) {

	// Init INIFile structure
	mINI::INIFile file(filename);
	mINI::INIStructure ini;

	// Actuall read the config file	
	bool read_success = file.read(ini);
	if (!read_success) {
		std::cerr << "FATAL ERROR: Impossible to open config file " << filename << std::endl;
		throw std::logic_error("FATAL ERROR: Impossible to open config file.");
	}
	
	// Extract all the sections / collections and put it into the `internal` class member
	for (auto const& it : ini)
	{
		auto const& section = it.first;
		auto const& collection = it.second;

		for (auto const& it2 : collection) {
			auto const& key = it2.first;
			auto const& value = it2.second;
			internal[{section, key}] = value;
		}
		
	}
	
}

template <typename T>
auto string_to(const std::string &from) {
	if constexpr(std::is_same<T, int>::value) {
		return std::stoi(from);
	} else if constexpr(std::is_same<T, long>::value) {
		return std::stol(from);
	} else if constexpr(std::is_same<T, long long>::value) {
		return std::stoll(from);
	} else if constexpr(std::is_same<T, float>::value) {
		return std::stof(from);
	} else if constexpr(std::is_same<T, double>::value) {
		return std::stod(from);
	}
}

template<>
std::string Config::read(const std::string &section, const std::string &key) const {
	try {
		return internal.at({section, key});
	} catch(const std::out_of_range &oor) {
		std::cerr << "FATAL ERROR: Configuration directive [" << section << "]/" << key
		          << " is missing." << std::endl;
		throw oor;
	}
}

template<typename T>
T Config::read(const std::string &section, const std::string &key) const {
	try {
		return string_to<T>(Config::read<std::string>(section, key));
	} catch(const std::logic_error &le) {
		std::cerr << "FATAL ERROR: Configuration directive [" << section << "]/" << key
		          << " must be a " << typeid(T).name() << " but is not." << std::endl;
		throw le;
	}
}

template std::string Config::read<std::string>(const std::string &section, const std::string &key) const;
template int Config::read<int>(const std::string &section, const std::string &key) const;
template long Config::read<long>(const std::string &section, const std::string &key) const;
template long long Config::read<long long>(const std::string &section, const std::string &key) const;
template float Config::read<float>(const std::string &section, const std::string &key) const;
template double Config::read<double>(const std::string &section, const std::string &key) const;

} // namespace konro
