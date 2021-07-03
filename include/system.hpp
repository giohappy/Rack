#pragma once
#include <list>
#include <vector>

#include <common.hpp>


namespace rack {


/** Cross-platform functions for operating systems routines
*/
namespace system {


// Filesystem

/** Joins two paths with a directory separator.
If `path2` is an empty string, returns `path1`.
*/
std::string join(const std::string& path1, const std::string& path2 = "");
/** Join an arbitrary number of paths, from left to right. */
template <typename... Paths>
std::string join(const std::string& path1, const std::string& path2, Paths... paths) {
	return join(join(path1, path2), paths...);
}
/** Returns a list of all entries (directories, files, symbolic links, etc) in a directory.
`depth` is the number of directories to recurse. 0 depth does not recurse. -1 depth recurses infinitely.
*/
std::list<std::string> getEntries(const std::string& dirPath, int depth = 0);
bool exists(const std::string& path);
/** Returns whether the given path is a file. */
bool isFile(const std::string& path);
/** Returns whether the given path is a directory. */
bool isDir(const std::string& path);
uint64_t getFileSize(const std::string& path);
/** Moves a file or directory.
Does not overwrite the destination. If this behavior is needed, use remove() or removeRecursively() before moving.
*/
void rename(const std::string& srcPath, const std::string& destPath);
/** Copies a file or directory recursively. */
void copy(const std::string& srcPath, const std::string& destPath);
/** Creates a directory.
The parent directory must exist.
*/
bool createDir(const std::string& path);
/** Creates all directories up to the path.
*/
bool createDirs(const std::string& path);
/** Deletes a file or empty directory.
Returns whether the deletion was successful.
*/
bool remove(const std::string& path);
/** Deletes a file or directory recursively.
Returns the number of files and directories that were deleted.
*/
int removeRecursively(const std::string& path);
std::string getWorkingDir();
void setWorkingDir(const std::string& path);
std::string getTempDir();
/** Returns the absolute path beginning with "/". */
std::string getAbsolute(const std::string& path);
/** Returns the canonical (unique) path, following symlinks and "." and ".." fake directories.
The path must exist on the filesystem.
Examples:
	getCanonical("/foo/./bar/.") // "/foo/bar"
*/
std::string getCanonical(const std::string& path);
/** Extracts the parent directory of the path.
Examples:
	getDir("/var/tmp/example.txt") // "/var/tmp"
	getDir("/") // ""
	getDir("/var/tmp/.") // "/var/tmp"
*/
std::string getDir(const std::string& path);
/** Extracts the filename of the path.
Examples:
	getFilename("/foo/bar.txt") // "bar.txt"
	getFilename("/foo/.bar") // ".bar"
	getFilename("/foo/bar/") // "."
	getFilename("/foo/.") // "."
	getFilename("/foo/..") // ".."
	getFilename(".") // "."
	getFilename("..") // ".."
	getFilename("/") // "/"
*/
std::string getFilename(const std::string& path);
/** Extracts the portion of a filename without the extension.
Examples:
	getStem("/foo/bar.txt") // "bar"
	getStem("/foo/.bar") // ""
	getStem("/foo/foo.tar.ztd") // "foo.tar"
*/
std::string getStem(const std::string& path);
/** Extracts the extension of a filename, including the dot.
Examples:
	getExtension("/foo/bar.txt") // ".txt"
	getExtension("/foo/bar.") // "."
	getExtension("/foo/bar") // ""
	getExtension("/foo/bar.txt/bar.cc") // ".cc"
	getExtension("/foo/bar.txt/bar.") // "."
	getExtension("/foo/bar.txt/bar") // ""
	getExtension("/foo/.") // ""
	getExtension("/foo/..") // ""
	getExtension("/foo/.hidden") // ".hidden"
*/
std::string getExtension(const std::string& path);

/** Compresses the contents of a directory (recursively) to an archive.
Uses the Unix Standard TAR + Zstandard format (.tar.zst).
An equivalent shell command is

	ZSTD_CLEVEL=1 tar -cf archivePath --zstd -C dirPath .
*/
void archiveDir(const std::string& archivePath, const std::string& dirPath, int compressionLevel = 1);
std::vector<uint8_t> archiveDir(const std::string& dirPath, int compressionLevel = 1);
/** Extracts an archive into a directory.
An equivalent shell command is

	tar -xf archivePath --zstd -C dirPath
*/
void unarchiveToDir(const std::string& archivePath, const std::string& dirPath);
void unarchiveToDir(const std::vector<uint8_t>& archiveData, const std::string& dirPath);


// Threading

/** Returns the number of logical simultaneous multithreading (SMT) (e.g. Intel Hyperthreaded) threads on the CPU. */
int getLogicalCoreCount();
/** Sets a name of the current thread for debuggers and OS-specific process viewers. */
void setThreadName(const std::string& name);

// Querying

/** Returns the caller's human-readable stack trace with "\n"-separated lines. */
std::string getStackTrace();
/** Returns the number of seconds since application launch.
Gives the most precise (fine-grained) time differences available on the OS for benchmarking purposes, while being fast to compute.
*/
double getTime();
/** Returns time since 1970-01-01 00:00:00 UTC in seconds.
*/
double getUnixTime();
std::string getOperatingSystemInfo();

// Applications

/** Opens a URL in a browser.
Shell injection is possible, so make sure the URL is trusted or hard coded.
May block, so open in a new thread.
*/
void openBrowser(const std::string& url);
/** Opens Windows Explorer, Finder, etc at a directory location. */
void openDir(const std::string& path);
/** Runs an executable without blocking.
The launched process will continue running if the current process is closed.
*/
void runProcessDetached(const std::string& path);

void init();


} // namespace system
} // namespace rack
