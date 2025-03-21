/**
 * @file Platform/File.h
 */

#pragma once

#include <list>
#include <string>
#include <vector>

/**
 * This class provides basic file input/output capabilities.
 */
class File
{
private:
  void* stream = nullptr; /**< File handle. */
  std::string fullName; /**< The full path name actually used or the file searched for if it was not found. */

public:
  /**
   * @param name File name or path. If it is a relative path, it is assumed
   *             to be relative to the path for configuration files. Otherwise,
   *             the path is used directly.
   * @param mode File open mode as used by fopen defined in stdio.h.
   * @param tryAlternatives Try alternative paths.
   */
  File(const std::string& name, const char* mode, bool tryAlternatives = true);

  ~File();

  /**
   * The method returns a list of full file names that should be searched
   * to find the file with a given name.
   * @param name The name of the file to search for.
   * @return The list of candidate full names. The caller has to check whether
   *         these files exist in the sequence of the list.
   */
  static std::list<std::string> getFullNames(const std::string& name);

  /**
   * The function read a number of bytes from the file to a certain
   * memory location.
   * @param p The start of the memory space the data is written to.
   * @param size The number of bytes read from the file.
   */
  void read(void* p, size_t size);

  /**
   * The function read a line (up to \c size bytes long) from the file to a certain
   * memory location.
   * @param p The start of the memory space the data is written to.
   * @param size The maximal length of the line read from the file.
   * @return \c p on success, \c 0 otherwise
   */
  char* readLine(char* p, size_t size);

  /**
   * The function writes a number of bytes from a certain memory
   * location into the file.
   * @param p The start of the memory space the data is read from.
   * @param size The number of bytes written into the file.
   */
  void write(const void* p, size_t size);

  /**
   * Skip a number of bytes in the stream.
   * @param size The amount of bytes to skip.
   */
  void skip(size_t size);

  /**
   * The function implements printf for the stream represented by
   * instances of this class.
   * @param format Format string as used by printf defined in stdio.h.
   * @param ... See printf in stdio.h.
   */
  void printf(const char* format, ...);

  /**
   * The function returns whether the file represented by an
   * object of this class actually exists.
   * @return The existence of the file.
   */
  bool exists() const { return stream != nullptr; }

  /**
   * The function returns whether the end of the file represented
   * by an object of this class was reached.
   * @return End of file reached?
   */
  bool eof();

  /**
   * The function returns the size of the file
   * @return The size of the file in bytes
   */
  size_t getSize();

  /**
   * The function returns the current position in the file
   * @return The current position in number of bytes since the begin
   */
  size_t getPosition();

  /**
   * The function returns the full path of the file.
   * @return The full path name actually used or the file searched for
   *         if it was not found. If the file was opened, the path can
   *         still be relative to the current directory if the B-Human
   *         directory was specified this way.
   */
  std::string getFullName() const { return fullName; }

  /**
   * The function returns the underlying native file handle
   * (currently always a \c FILE pointer).
   * @return The native file handle.
   */
  void* getNativeFile() const { return stream; }

  /**
   * The function returns the current BH directory,
   * e.g. /home/nao or <..>/B-Human
   * @return The current BHDir
   */
  static const char* getBHDir();

  /**
   * Checks whether a path is an absolute path.
   * Empty paths are handled as relative paths.
   * @param path The path.
   * @return true, if the path is absolute.
   */
  static bool isAbsolute(const std::string& path);

  /**
   * Checks whether the path name has an extension.
   * @param path The path.
   * @return true, if the path has an extension.
   */
  static bool hasExtension(const std::string& path);

  /**
   * Sets the search path for \c getConfigDirs.
   * @param paths The new search path.
   */
  static void setSearchPath(const std::vector<std::string>& paths);

  /** Clears the search path. */
  static void clearSearchPath();

private:
  static thread_local std::vector<std::string> searchPath;
};
