#ifndef WSJTX_NATIVE_SHARED_MEMORY_HPP
#define WSJTX_NATIVE_SHARED_MEMORY_HPP

#include <cstddef>
#include <string>

class NativeSharedMemory final
{
public:
  NativeSharedMemory () = default;
  ~NativeSharedMemory ();
  NativeSharedMemory (NativeSharedMemory const&) = delete;
  NativeSharedMemory& operator= (NativeSharedMemory const&) = delete;

  bool create (std::string const& name, std::size_t bytes);
  bool attach (std::string const& name);
  bool detach () noexcept;
  static bool remove (std::string const& name, std::string * error = nullptr);

  void * data () const noexcept { return data_; }
  std::size_t size () const noexcept { return size_; }
  bool isAttached () const noexcept { return data_ != nullptr; }
  bool alreadyExists () const noexcept;
  bool notFound () const noexcept;
  std::string errorString () const;

private:
  bool begin (std::string const& name);
  bool fail (char const * operation, unsigned long code) noexcept;

  std::string name_;
  void * data_ {nullptr};
  std::size_t size_ {0};
  bool owner_ {false};
  char const * operation_ {nullptr};
  unsigned long error_ {0};
#ifdef _WIN32
  void * handle_ {nullptr};
#else
  int descriptor_ {-1};
#endif
};

#endif
