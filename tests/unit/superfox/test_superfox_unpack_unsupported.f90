module superfox_unsupported_fixture
  use iso_c_binding, only: c_ptr,c_f_pointer,c_int
  use decoder_engine_types, only: superfox_observation
  implicit none
contains
  subroutine observe(context,record)
    type(c_ptr), intent(in) :: context
    type(superfox_observation), intent(in) :: record
    integer(c_int), pointer :: n
    call c_f_pointer(context,n)
    n=n+1
    write(*,*) 'unexpected child kind for unsupported frame:',record%kind
  end subroutine observe
end module superfox_unsupported_fixture

program test_superfox_unpack_unsupported

  use iso_c_binding, only: c_loc,c_int
  use superfox_unsupported_fixture, only: observe
  use packjt77, only: pack77_state
  use sfox_unpack_module, only: sfox_unpack_for_state
  implicit none

  integer*1 xin(0:49), xdec(0:49)
  logical*1 more_cqs, send_msg
  character*120 line
  character*26 free_text
  character*10 ckey
  character*13 foxcall
  character*329 msgbits
  integer notp, pack_error
  integer(c_int), target :: count=0
  type(pack77_state) :: knowledge

  line='K1JT W1AW'
  free_text=' '
  ckey='0000123456'
  more_cqs=.false.
  send_msg=.false.

  call sfox_pack(line,ckey,more_cqs,send_msg,free_text,xin,pack_error)
  if(pack_error.ne.0) error stop 'sfox_pack failed'

  xdec=xin(49:0:-1)
  write(msgbits,1000) xdec(0:46)
1000 format(47b7.7)
  msgbits(327:329)='001'
  read(msgbits,1001) xdec(0:46)
1001 format(47b7)

  notp=0
  foxcall=''
  call sfox_unpack(123456,xdec,0,1500.0,0.0,foxcall,notp)
  call sfox_unpack_for_state(knowledge,observe,c_loc(count),123456,xdec,0,1500.0,0.0,foxcall,notp)
  if(count.ne.0) error stop 'unsupported frame emitted typed children'
  write(*,*) 'unsupported i3=1 ignored'
end program test_superfox_unpack_unsupported
