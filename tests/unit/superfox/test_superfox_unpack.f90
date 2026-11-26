module superfox_unpack_fixture
  use iso_c_binding, only: c_ptr,c_f_pointer,c_int
  use decoder_engine_types, only: superfox_observation
  implicit none
  integer*1 :: xdec(0:49)
  type(superfox_observation) :: records(12)
contains
  subroutine observe(context,record)
    type(c_ptr), intent(in) :: context
    type(superfox_observation), intent(in) :: record
    integer(c_int), pointer :: count
    character(len=160) expected

    call c_f_pointer(context,count)
    count=count+1
    if(count.gt.size(records)) error stop 'unexpected extra child'
    if(record%child_index.ne.count) error stop 'child order changed'
    if(any(record%symbols.ne.xdec)) error stop 'accepted QPC frame not preserved'
    write(expected,'(i6.6,i4,f5.1,i5,1x,"~",2x,a)') &
         123456,record%snr_db,record%dt_seconds,nint(record%frequency_hz),trim(record%message)
    if(trim(record%legacy_line).ne.trim(expected)) error stop 'legacy formatting changed'
    records(count)=record
  end subroutine observe
end module superfox_unpack_fixture

program test_superfox_unpack

  use iso_c_binding, only: c_loc,c_int
  use superfox_unpack_fixture, only: observe,xdec,records
  use packjt77, only: pack77_state
  use sfox_pack_module, only: sfox_pack_for_state
  use sfox_unpack_module, only: sfox_unpack_for_state,sfox_render_legacy
  use decoder_engine_types, only: sf_kind_cq,sf_kind_exchange, &
       sf_kind_free_text,sf_kind_verification
  implicit none

  integer*1 xin(0:49)
  logical*1 more_cqs, send_msg
  character*120 line
  character*26 free_text
  character*10 ckey
  character*13 foxcall
  integer notp, pack_error
  integer(c_int), target :: observed_count=0
  type(pack77_state) :: knowledge

  line='CQ K1JT FN20'
  free_text='2S/IZFUZBRXJF             '
  ckey='0000123456'
  more_cqs=.false.
  send_msg=.true.

  call sfox_pack_for_state(knowledge,line,ckey,more_cqs,send_msg,free_text,xin,pack_error)
  if(pack_error.ne.0) error stop 'sfox_pack failed'

  xdec=xin(49:0:-1)

  notp=0
  foxcall=''
  call sfox_unpack_for_state(knowledge,observe,c_loc(observed_count),123456,xdec,0,1500.0,0.0,foxcall,notp)
  if(observed_count.ne.2) error stop 'CQ frame must produce CQ and free-text children'
  if(records(1)%kind.ne.sf_kind_cq .or. records(2)%kind.ne.sf_kind_free_text) &
       error stop 'incorrect CQ frame child kinds'
  if(records(1)%message.ne.'CQ K1JT FN20') error stop 'incorrect CQ rendering'
  if(records(2)%message.ne.'2S/IZFUZBRXJF') error stop 'incorrect free-text rendering'
  if(records(2)%legacy_line_length.ne.50) error stop 'free-text padding changed'
  call sfox_render_legacy(c_loc(observed_count),records(1))
  call sfox_render_legacy(c_loc(observed_count),records(2))

  observed_count=0
  notp=1
  call sfox_unpack_for_state(knowledge,observe,c_loc(observed_count),123456,xdec,0,1500.0,0.0,foxcall,notp)
  if(observed_count.ne.3) error stop 'verification must be a separate child'
  if(records(3)%kind.ne.sf_kind_verification) error stop 'verification classified as decoded QSO'
  if(index(records(3)%message,'$VERIFY$ K1JT ').ne.1) error stop 'verification text changed'

  line='K1JT W1AW K9AN -12'
  send_msg=.false.
  free_text=' '
  call sfox_pack_for_state(knowledge,line,ckey,more_cqs,send_msg,free_text,xin,pack_error)
  if(pack_error.ne.0) error stop 'exchange pack failed'
  xdec=xin(49:0:-1)
  notp=0
  observed_count=0
  call sfox_unpack_for_state(knowledge,observe,c_loc(observed_count),123456,xdec,-8,1500.0,0.2,foxcall,notp)
  if(observed_count.ne.2) error stop 'exchange frame child count changed'
  if(any(records(1:2)%kind.ne.sf_kind_exchange)) error stop 'exchange kind changed'
  if(records(1)%message.ne.'W1AW K1JT RR73') error stop 'RR73 exchange changed'
  if(records(2)%message.ne.'K9AN K1JT -12') error stop 'report exchange changed'

end program test_superfox_unpack
