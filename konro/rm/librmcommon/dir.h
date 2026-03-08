#ifndef DIR_H
#define DIR_H

#include <vector>
#include <string>
#include <iterator>

namespace rmcommon {
/*!
 * \class a class for handling directories
 */
class Dir {
public:
	class DirEntry {
		std::string name_;
		short flags_;
		unsigned long size_;
	public:
		enum { DE_REG = 1, DE_DIR = 2 };
		DirEntry(const char *path, short flg, unsigned long sz = 0) :
			name_(path),
			flags_(flg),
			size_(sz)
		{
		}
		/*! Returns the file or directory name */
		const std::string &name() const { return name_; }
		/*! Returns true if the entry is a regular file */
		bool is_reg() const { return flags_ & DE_REG; }
		/*! Return true if the entry is a directory */
		bool is_dir() const { return flags_ & DE_DIR; }
		/*! Return the size in bytes (0 for directories) */
		unsigned long size() const { return size_; }

		friend class Dir;
	};

	/*!
	 * A minimal forward iterator for the Dir class
	 * \p
	 * You can write code like:
	 * \code
	 *     dir::Dir directory = dir::Dir::localdir(my_path);
	 *     for (const auto &de: directory) {
	 *         ...
	 *     }
	 * \endcode
	 */
	class DirIterator: public std::iterator<std::forward_iterator_tag, DirEntry> {
		DirEntry *p_;
	public:
		typedef DirEntry value_type;
		typedef ptrdiff_t difference_type;
		typedef DirEntry *pointer;
		typedef DirEntry &reference;

		DirIterator(DirEntry *p) : p_(p) {}
		DirIterator(const DirIterator &other) { p_ = other.p_; }
		DirIterator &operator =(const DirIterator &other) { p_ = other.p_; return *this; }
		DirIterator& operator++() { ++p_; return *this; }
		DirIterator operator++(int) { DirIterator tmp(*this); operator++(); return tmp; }
		bool operator==(const DirIterator& rhs) const { return p_ == rhs.p_; }
		bool operator!=(const DirIterator& rhs) const { return p_ != rhs.p_; }
		DirEntry &operator*() { return *p_; }
		DirEntry *operator->() { return p_; }
	};

	typedef DirIterator iterator;

	explicit Dir(const char *dirname);

	DirIterator begin() { return DirIterator(&entries[0]); }
	DirIterator end() { return DirIterator(&entries[entries.size()]); }

	void add_entry(const DirEntry &de)
	{
		entries.push_back(de);
	}

    static bool dir_exists(const char *path);
    static bool file_exists(const char *path);
	static Dir localdir(const char *path);
    static std::string home();

    /*!
     * Creates the specified directory
     *
     * \param path the path of the directory
     * \throws std::runtime_error in case of error
     */
    static void mkdir(const char *path);

    /*!
     * Recursively creates all the directories of a path
     * \param path the path containing the folders to create
     * \throws std::runtime_error in case of error
     */
    static void mkdir_r(const char *path);

    /*!
     * \brief Removes the directory "path"
     * \param path the name of the directory
     * \throws std::runtime_error in case of error
     */
    static void rmdir(const char *path);

private:
	std::string dirname;
	std::vector<DirEntry> entries;
};

}	// namespace pc

#endif	// DIR_H
