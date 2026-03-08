#include "dir.h"
#include "makepath.h"
#include "tsplit.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SEPARATOR		'/'

using namespace std;

namespace {

    /*!
     * \brief Creates a directory if it does not exist
     * \param path the directory path
     * \param create_mode
     */
    void create_dir(const char *path, mode_t create_mode)
    {
        if (!rmcommon::Dir::dir_exists(path)) {
            errno = 0;
            if (::mkdir(path, create_mode) != 0 && errno != EEXIST) {
                ostringstream os;
                os << "Could not create directory "
                   << path
                   << ": " << strerror(errno);
                throw runtime_error(os.str());
            }
        }
    }
}

namespace rmcommon {

Dir::Dir(const char *dirname) :
	dirname(dirname)
{
}

/*!
 * \brief Dir::dir_exists
 * \param path
 * \return true if 'path' is an existing directory
 */
bool Dir::dir_exists(const char *path)
{
    struct stat sbuf;
    if (lstat(path, &sbuf) != 0) {
        return false;
    }
    return S_ISDIR(sbuf.st_mode);
}

bool Dir::file_exists(const char *path)
{
    struct stat statbuf;

    if (stat(path, &statbuf) != 0)
        return false;
    return S_ISREG(statbuf.st_mode);
}

/*static*/
Dir Dir::localdir(const char *path)
{
	DIR *dir = opendir(path);
	if (dir == nullptr) {
        throw runtime_error(string("Dir::localdir: could not open path ") + path);
	}
    Dir directory(path);
	struct dirent *d;
	struct stat sbuf;
	while ((d = readdir(dir)) != nullptr) {
		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;

        string local_file = make_path(path, d->d_name);

		// check that it is really a file or a directory
		memset(&sbuf, 0, sizeof sbuf);
		errno = 0;
		// note that lstat does not follow symbolic links
		if (lstat(local_file.c_str(), &sbuf) != 0) {
			cerr << "lstat failed for " << local_file << " " << strerror(errno) << endl;
			continue;
		}
		if (S_ISDIR(sbuf.st_mode)) {
            directory.add_entry(Dir::DirEntry(d->d_name, Dir::DirEntry::DE_DIR));
		} else if (S_ISREG(sbuf.st_mode)) {
            directory.add_entry(Dir::DirEntry(d->d_name, Dir::DirEntry::DE_REG, sbuf.st_size));
		} else {
			//cout << "Skipping " << local_file << " (not a regular file or directory)" << endl;
		}
	}

	closedir(dir);
    return directory;
}

string Dir::home()
{
    const char *home = getenv("HOME");
    return home ? home : "";
}

void Dir::mkdir(const char *path)
{
    mode_t create_mode = S_IRWXU|S_IRWXG|S_IRWXO;
    create_dir(path, create_mode);
}

void Dir::mkdir_r(const char *path)
{
	if (path == nullptr || path[0] == '\0')
        return;
#if 1
    // ensure that memory is deleted
    std::unique_ptr<char[]> upath(new char[strlen(path)+1]);
    char *path2 = upath.get();
    if (path2 == nullptr)
        return;
    strcpy(path2, path);
    char *p = path2;
    bool first = true;
    bool last = false;
    mode_t numask, oumask = 0;
    //cout << ">>> Creating path '" << path2 << '\'' << endl;
    if (*p == '/')
        ++p;        // skip initial '\'
    for(; !last; ++p) {
        if (*p == '\0')
            last = true;
        else if (*p != '/')
            continue;
        else
            *p = '\0';
        if (!last && p[1] == '\0')
            last = true;
        if (first) {
            oumask = umask(0);
            numask = oumask & ~(S_IWUSR | S_IXUSR);
            umask(numask);
            first = false;
        }
        if (last)
            umask(oumask);
        //cout << ">>> Creating directory '" << path2 << '\'' << endl;
        create_dir(path2, S_IRWXU|S_IRWXG|S_IRWXO);
        if (!last)
            *p = '/';
    }

#else
    vector<string> directories = rmcommon::tsplit(string(path), "/");

	if (directories.size() == 0)
        return;

	mode_t create_mode = S_IRWXU|S_IRWXG|S_IRWXO;
	string curdir;
    curdir = directories[0];
    if (!curdir.empty()) {
        // path was a relative path (i.e. did not start with '/')
        create_dir(curdir.c_str(), create_mode);
    }
	for (size_t i = 1; i < directories.size(); ++i) {
        curdir += SEPARATOR + directories[i];
        create_dir(curdir.c_str(), create_mode);
    }
#endif
}

void Dir::rmdir(const char *path)
{
    errno = 0;
    if (::rmdir(path) != 0) {
        ostringstream os;
        os << "Could not remove directory "
           << path
           << ": " << strerror(errno);
        throw runtime_error(os.str());
    }
}

}	// namespace pc
