#include "DecoderWorkerLock.hpp"

#include <cerrno>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

DecoderWorkerLock::DecoderWorkerLock (char const * path) noexcept
{
#ifndef _WIN32
  if (!path || !*path) { error_ = EINVAL; return; }
  do
    {
      descriptor_ = open (path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    }
  while (descriptor_ == -1 && errno == EINTR);
  if (descriptor_ == -1) error_ = errno;
#else
  (void) path;
#endif
}

DecoderWorkerLock::~DecoderWorkerLock ()
{
#ifndef _WIN32
  // Keep the inode: unlinking it would let another worker lock a different file.
  if (descriptor_ != -1) close (descriptor_);
#endif
}

bool DecoderWorkerLock::tryLock () noexcept
{
#ifndef _WIN32
  if (descriptor_ == -1) return false;
  int result;
  do { result = flock (descriptor_, LOCK_EX | LOCK_NB); }
  while (result == -1 && errno == EINTR);
  error_ = result == -1 ? errno : 0;
#endif
  return error_ == 0;
}

bool DecoderWorkerLock::busy () const noexcept
{
  return error_ == EWOULDBLOCK || error_ == EAGAIN;
}

std::string DecoderWorkerLock::errorString () const
{
  return std::system_category ().message (error_);
}
