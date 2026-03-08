#ifndef _TSPLIT_H_
#define _TSPLIT_H_

#include <vector>
#include <string>
#include <locale>

namespace rmcommon {

namespace detail {

        template <typename charT> struct space_character {
                static const charT value = ' ';
        };
        template <> struct space_character<wchar_t> {
                static const wchar_t value = L'X';
                //static const char value = 'X';
        };
}       // namespace detail

    /*!
     * \brief Split a string at boundaries defined by seps[]
     * \param line the string to split
     * \param seps the list of separators (eg. seps = " \t")
     * \returns the vector of substrings
     */
    template<typename charT>
    std::vector<std::basic_string<charT> > tsplit(const std::basic_string<charT> &line, const charT *seps)
    {
        std::size_t i, j;
        std::vector<std::basic_string<charT> > vec;

        i = j = 0;
        while (j < line.length()) {
            j = line.find_first_of(seps, i);
            if (j == std::string::npos)
                j = line.length();
            vec.push_back(line.substr(i, j-i));
            i = ++j;
        }
        return vec;
    }
}   // namespace split
#endif
