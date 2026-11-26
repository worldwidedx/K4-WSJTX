#include "NativeSharedMemory.hpp"
#include "DecoderWorkerLock.hpp"
#include <memory>
#include <cstdio>
#include <exception>

namespace
{
  // Release the mapping before the lifetime lock, after all Fortran cleanup.
  std::unique_ptr<DecoderWorkerLock> worker_lock;
  NativeSharedMemory shmem;
}

extern "C"
{
  bool shmem_lock_worker (char const * path) noexcept
  {
    try
      {
        worker_lock.reset (new DecoderWorkerLock {path});
        if (worker_lock->tryLock ()) return true;
        std::fprintf (stderr, "Decoder worker lock: %s\n", worker_lock->errorString ().c_str ());
      }
    catch (...)
      {
        std::fputs ("Decoder worker lock: initialization failed\n", stderr);
      }
    return false;
  }

  bool shmem_attach (char const * name) noexcept
  {
    try
      {
        if (shmem.attach (name)) return true;
        std::fprintf (stderr, "Decoder shared memory: %s\n", shmem.errorString ().c_str ());
      }
    catch (std::exception const& error)
      {
        std::fprintf (stderr, "Decoder shared memory: %s\n", error.what ());
      }
    catch (...)
      {
        std::fputs ("Decoder shared memory: unexpected attachment failure\n", stderr);
      }
    return false;
  }
  std::size_t shmem_size () noexcept {return shmem.size ();}
  void * shmem_address () noexcept {return shmem.data ();}
}
