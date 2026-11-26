module shmem
  interface
     function shmem_attach (key) bind(C, name="shmem_attach")
       use iso_c_binding, only: c_bool, c_char
       character(kind=c_char), intent(in) :: key(*)
       logical(c_bool) :: shmem_attach
     end function shmem_attach

     function shmem_address() bind(C, name="shmem_address")
       use, intrinsic :: iso_c_binding, only: c_ptr
       type(c_ptr) :: shmem_address
     end function shmem_address

     function shmem_size() bind(C, name="shmem_size")
       use, intrinsic :: iso_c_binding, only: c_size_t
       integer(c_size_t) :: shmem_size
     end function shmem_size

     function shmem_lock_worker (path) bind(C, name="shmem_lock_worker")
       use iso_c_binding, only: c_bool, c_char
       character(kind=c_char), intent(in) :: path(*)
       logical(c_bool) :: shmem_lock_worker
     end function shmem_lock_worker
  end interface
end module shmem
