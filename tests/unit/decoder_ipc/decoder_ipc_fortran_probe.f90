subroutine decoder_ipc_fortran_probe(address, values) bind(C)
  use, intrinsic :: iso_c_binding, only: c_ptr, c_f_pointer, c_int, c_sizeof
  include 'jt9com.f90'

  type(c_ptr), value :: address
  integer(c_int), intent(out) :: values(11)
  type(shared_dec_data), pointer :: shared

  call c_f_pointer(address, shared)
  values(1) = c_sizeof(shared%control)
  values(2) = c_sizeof(shared%payload%params)
  values(3) = c_sizeof(shared%payload)
  values(4) = c_sizeof(shared)
  values(5) = DECODER_IPC_VERSION
  values(6) = DECODER_IPC_IDLE
  values(7) = DECODER_IPC_READY
  values(8) = DECODER_IPC_DECODING
  values(9) = DECODER_IPC_COMPLETE
  values(10) = DECODER_IPC_SHUTDOWN
  values(11) = c_sizeof(shared%metadata)

  shared%control%generation = shared%control%generation + 1
  shared%payload%id2(1) = 1234
  shared%metadata%input_id = 12345678901_c_int64_t
  shared%metadata%analysis_id = 12345678902_c_int64_t
  shared%metadata%attempt_no = 3
  shared%metadata%valid_samples = 180000
end subroutine decoder_ipc_fortran_probe
