#include "NativeSharedMemory.hpp"

#include <cstdint>
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
  bool valid_name (std::string const& name)
  {
#ifdef _WIN32
    std::string const prefix {"Local\\wsjt-"};
#else
    std::string const prefix {"/wsjt-"};
#endif
    if (name.size () != prefix.size () + 24
        || name.compare (0, prefix.size (), prefix) != 0) return false;
    for (auto i = prefix.size (); i < name.size (); ++i)
      if (!((name[i] >= '0' && name[i] <= '9')
            || (name[i] >= 'a' && name[i] <= 'f'))) return false;
    return true;
  }

  std::string native_error (char const * operation, unsigned long code)
  {
    if (!operation) return {};
    return std::string {operation} + ": "
      + std::system_category ().message (static_cast<int> (code))
      + " (" + std::to_string (code) + ")";
  }
}

NativeSharedMemory::~NativeSharedMemory ()
{
  detach ();
}

bool NativeSharedMemory::fail (char const * operation, unsigned long code) noexcept
{
  operation_ = operation;
  error_ = code;
  return false;
}

bool NativeSharedMemory::begin (std::string const& name)
{
#ifdef _WIN32
  if (data_ || handle_) return fail ("shared memory already attached", ERROR_BUSY);
  if (!valid_name (name)) return fail ("invalid shared memory name", ERROR_INVALID_NAME);
#else
  if (data_ || descriptor_ != -1) return fail ("shared memory already attached", EBUSY);
  if (!valid_name (name)) return fail ("invalid shared memory name", EINVAL);
#endif
  name_ = name;
  operation_ = nullptr;
  error_ = 0;
  return true;
}

bool NativeSharedMemory::create (std::string const& name, std::size_t bytes)
{
  if (!begin (name)) return false;
#ifdef _WIN32
  if (!bytes) return fail ("invalid shared memory size", ERROR_INVALID_PARAMETER);
  std::wstring const wide_name {name.begin (), name.end ()};
  auto const length = static_cast<std::uint64_t> (bytes);
  handle_ = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                               static_cast<DWORD> (length >> 32),
                               static_cast<DWORD> (length), wide_name.c_str ());
  auto const error = GetLastError ();
  if (!handle_) return fail ("CreateFileMappingW", error);
  if (error == ERROR_ALREADY_EXISTS)
    {
      CloseHandle (handle_);
      handle_ = nullptr;
      return fail ("CreateFileMappingW", error);
    }
  data_ = MapViewOfFile (handle_, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  if (!data_)
    {
      auto const map_error = GetLastError ();
      detach ();
      return fail ("MapViewOfFile", map_error);
    }
#else
  if (!bytes || bytes > static_cast<std::uintmax_t> (std::numeric_limits<off_t>::max ()))
    return fail ("invalid shared memory size", EINVAL);
  descriptor_ = shm_open (name.c_str (), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (descriptor_ == -1) return fail ("shm_open (create)", errno);
  owner_ = true;
  if (ftruncate (descriptor_, static_cast<off_t> (bytes)) == -1)
    {
      auto const error = errno;
      detach ();
      return fail ("ftruncate", error);
    }
#ifdef __linux__
  int allocation_error;
  do { allocation_error = posix_fallocate (descriptor_, 0, static_cast<off_t> (bytes)); }
  while (allocation_error == EINTR);
  if (allocation_error)
    {
      detach ();
      return fail ("posix_fallocate", allocation_error);
    }
#endif
  auto * address = mmap (nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor_, 0);
  if (address == MAP_FAILED)
    {
      auto const error = errno;
      detach ();
      return fail ("mmap", error);
    }
  data_ = address;
#endif
  size_ = bytes;
  owner_ = true;
  return true;
}

bool NativeSharedMemory::attach (std::string const& name)
{
  if (!begin (name)) return false;
#ifdef _WIN32
  std::wstring const wide_name {name.begin (), name.end ()};
  handle_ = OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, wide_name.c_str ());
  if (!handle_) return fail ("OpenFileMappingW", GetLastError ());
  data_ = MapViewOfFile (handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (!data_)
    {
      auto const error = GetLastError ();
      detach ();
      return fail ("MapViewOfFile", error);
    }
  MEMORY_BASIC_INFORMATION information {};
  if (!VirtualQuery (data_, &information, sizeof information))
    {
      auto const error = GetLastError ();
      detach ();
      return fail ("VirtualQuery", error);
    }
  size_ = information.RegionSize;
#else
  descriptor_ = shm_open (name.c_str (), O_RDWR, 0);
  if (descriptor_ == -1) return fail ("shm_open (attach)", errno);
  struct stat information {};
  if (fstat (descriptor_, &information) == -1)
    {
      auto const error = errno;
      detach ();
      return fail ("fstat", error);
    }
  if (information.st_size <= 0
      || static_cast<std::uintmax_t> (information.st_size) > std::numeric_limits<std::size_t>::max ())
    {
      detach ();
      return fail ("invalid shared memory size", EINVAL);
    }
  auto const bytes = static_cast<std::size_t> (information.st_size);
  auto * address = mmap (nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, descriptor_, 0);
  if (address == MAP_FAILED)
    {
      auto const error = errno;
      detach ();
      return fail ("mmap", error);
    }
  data_ = address;
  size_ = bytes;
#endif
  return true;
}

bool NativeSharedMemory::detach () noexcept
{
  char const * operation {nullptr};
  unsigned long error {0};
  auto remember = [&] (char const * failed_operation, unsigned long code)
    {
      if (!operation) { operation = failed_operation; error = code; }
    };
#ifdef _WIN32
  if (data_ && !UnmapViewOfFile (data_)) remember ("UnmapViewOfFile", GetLastError ());
  if (handle_ && !CloseHandle (handle_)) remember ("CloseHandle", GetLastError ());
  handle_ = nullptr;
#else
  // The GUI instance lock serializes owner cleanup and stale-name replacement.
  if (owner_ && shm_unlink (name_.c_str ()) == -1 && errno != ENOENT)
    remember ("shm_unlink", errno);
  if (data_ && munmap (data_, size_) == -1) remember ("munmap", errno);
  if (descriptor_ != -1 && close (descriptor_) == -1) remember ("close", errno);
  descriptor_ = -1;
#endif
  data_ = nullptr;
  size_ = 0;
  owner_ = false;
  if (operation) return fail (operation, error);
  return true;
}

bool NativeSharedMemory::remove (std::string const& name, std::string * error)
{
  if (error) error->clear ();
#ifdef _WIN32
  (void) name;
  if (error) *error = "Windows shared memory is retired when its last handle closes";
  return false;
#else
  if (!valid_name (name))
    {
      if (error) *error = native_error ("invalid shared memory name", EINVAL);
      return false;
    }
  if (shm_unlink (name.c_str ()) == 0 || errno == ENOENT) return true;
  if (error) *error = native_error ("shm_unlink", errno);
  return false;
#endif
}

bool NativeSharedMemory::alreadyExists () const noexcept
{
#ifdef _WIN32
  return error_ == ERROR_ALREADY_EXISTS;
#else
  return error_ == EEXIST;
#endif
}

bool NativeSharedMemory::notFound () const noexcept
{
#ifdef _WIN32
  return error_ == ERROR_FILE_NOT_FOUND;
#else
  return error_ == ENOENT;
#endif
}

std::string NativeSharedMemory::errorString () const
{
  return native_error (operation_, error_);
}
