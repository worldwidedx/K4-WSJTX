#ifndef WSJTX_DECODER_WORKER_LOCK_HPP
#define WSJTX_DECODER_WORKER_LOCK_HPP

#include <string>

class DecoderWorkerLock final
{
public:
  explicit DecoderWorkerLock (char const * path) noexcept;
  ~DecoderWorkerLock ();
  DecoderWorkerLock (DecoderWorkerLock const&) = delete;
  DecoderWorkerLock& operator= (DecoderWorkerLock const&) = delete;

  bool tryLock () noexcept;
  bool busy () const noexcept;
  std::string errorString () const;

private:
  int descriptor_ {-1};
  int error_ {0};
};

#endif
