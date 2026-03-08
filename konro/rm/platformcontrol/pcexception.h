#ifndef PCEXCEPTION_H
#define PCEXCEPTION_H

#include <stdexcept>
#include <string>

namespace pc {

class PcException : public std::runtime_error {

public:
    PcException(const std::string &what) : std::runtime_error(what) {}
};

}

#endif // PCEXCEPTION_H
