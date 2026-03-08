#ifndef KEYVALUEPARSER_H
#define KEYVALUEPARSER_H

#include "numericvalue.h"
#include <string>
#include <map>
#include <utility>

namespace pc {

/*!
 * \brief The KeyValueParser class
 *
 * Parses lines in one of the following formats:
 * \code
 * key=value
 * key:value
 *
 * key=value key=value [key=value ...]
 * key:value key:value [key:value ...]
 *
 * major:minor key=value [key=value ...]
 * major:minor key:value key:value [key:value ...]
 * \endcode
 */
class KeyValueParser {
    const char *ptr_, *endptr_;

    /*!
     * \brief Parses a line in the format "major:minor ..."
     * \param major the major device number
     * \param minor the minor device number
     * \returns true if the line starts with the specified major and minor numbers,
     *          false otherwise
     * \throws PcException if the format of the line is invalid
     */
    bool parseMajorMinor(int major, int minor);

    /*!
     * Parses a key-value pair, such as key=value or key:value
     *
     * \returns the key-value pair
     * \throws PcException in case of format error
     */
    std::pair<std::string, rmcommon::NumericValue> parseKeyValue();

public:

    /*!
     * Parses a line in the format
     * \code
     * tag=value [tag=value ...]
     * \endcode
     * where value can be numeric or the string "max"
     *
     * \param line the line to parse
     * \return map of key and values
     * \exception PcException if the format of the line is invalid
     */
    std::map<std::string, rmcommon::NumericValue> parseLineNv(const char *line);
    std::map<std::string, rmcommon::NumericValue> parseLineNv(const std::string &line) {
        return parseLineNv(line.c_str());
    }

    /*!
     * Parses a line in the format
     * \code
     * major:minor tag=value [tag=value ...]
     * \endcode
     * where value can be numeric or the string "max"
     *
     * \param line the line to parse
     * \param major the expected device major number
     * \param minor the expected device minor number
     * \returns the map of key and values. If the major or minor device numbers
     *         of the line are different from the requested major and minor
     *         numbers, the map is empty
     * \throws PcException if the format of the line is invalid
     */
    std::map<std::string, rmcommon::NumericValue> parseLineNv(const char *line, int major, int minor);
    std::map<std::string, rmcommon::NumericValue> parseLineNv(const std::string &line, int major, int minor) {
        return parseLineNv(line.c_str(), major, minor);
    }

    /*!
     * Parses a single key-value pair, such as key=value or key:value
     * \param line the line to parse
     * \returns the key-value pair
     * \throws PcException in case of format error
     */
    std::pair<std::string, rmcommon::NumericValue> parseKeyValue(const char *line) {
        ptr_ = line;
        return parseKeyValue();
    }
    std::pair<std::string, rmcommon::NumericValue> parseKeyValue(const std::string &line) {
        return parseKeyValue(line.c_str());
    }
};

}   // namespace pc

#endif // KEYVALUEPARSER_H
