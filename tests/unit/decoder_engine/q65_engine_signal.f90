subroutine decoder_engine_test_q65_signal(period,submode,flag,seed,amplitude,noise,frequency,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t,c_float
  implicit none
  integer(c_int), value :: period,submode,flag,seed
  real(c_float), value :: amplitude,noise,frequency
  integer(c_int16_t), intent(out) :: samples(period*12000)
  call q65_engine_make_signal('K1ABC W9XYZ FN42',period,submode,flag,seed,amplitude,noise,frequency,samples)
end subroutine

subroutine decoder_engine_test_q65_message(message,period,submode,flag,seed,amplitude,noise,frequency,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t,c_float,c_char
  implicit none
  character(c_char), intent(in) :: message(37)
  integer(c_int), value :: period,submode,flag,seed
  real(c_float), value :: amplitude,noise,frequency
  integer(c_int16_t), intent(out) :: samples(period*12000)
  character(len=37) :: text
  integer :: i
  do i=1,37
     text(i:i)=message(i)
  enddo
  call q65_engine_make_signal(text,period,submode,flag,seed,amplitude,noise,frequency,samples)
end subroutine

subroutine q65_engine_make_signal(message,period,submode,flag,seed,amplitude,noise,frequency,samples)
  use iso_c_binding, only: c_int,c_int16_t,c_float
  use iso_fortran_env, only: int64,real64
  use packjt77, only: pack77_state
  use q65, only: q65_state,q65_work
  implicit none
  character(len=*), intent(in) :: message
  integer(c_int), intent(in) :: period,submode,flag,seed
  real(c_float), intent(in) :: amplitude,noise,frequency
  integer(c_int16_t), intent(out) :: samples(period*12000)
  real(real64), parameter :: twopi=6.283185307179586476925286766559_real64
  integer(int64) :: state
  integer :: tones(85),nsps,start,i,symbol,i3,n3
  real(real64) :: phase,value,u1,u2,freq
  character(len=37) :: encoded,sent
  type(pack77_state), target :: knowledge
  type(q65_state), allocatable, target :: encoder
  type(q65_state), pointer :: previous

  select case(period)
  case(15)
     nsps=1800
  case(30)
     nsps=3600
  case(60)
     nsps=7200
  case(120)
     nsps=16000
  case(300)
     nsps=41472
  case default
     error stop 'invalid Q65 fixture period'
  end select
  ! Fixture encoding must not populate the decoder callsign knowledge.
  allocate(encoder)
  encoder%knowledge=>knowledge
  previous=>q65_work
  q65_work=>encoder
  encoded=message
  call genq65(encoded,0,sent,tones,i3,n3,flag)
  q65_work=>previous
  if(i3<0.or.n3<0) error stop 'Q65 fixture message failed to encode'
  start=12000
  if(period.le.30) start=6000
  state=104729_int64+int(seed,int64)
  phase=0.0_real64
  do i=1,size(samples)
     state=modulo(16807_int64*state,2147483647_int64)
     u1=max(real(state,real64)/2147483647.0_real64,tiny(1.0_real64))
     state=modulo(16807_int64*state,2147483647_int64)
     u2=real(state,real64)/2147483647.0_real64
     value=noise*sqrt(-2.0_real64*log(u1))*cos(twopi*u2)
     if(i.gt.start .and. i.le.start+85*nsps) then
        symbol=(i-start-1)/nsps+1
        freq=frequency+tones(symbol)*(12000.0_real64/nsps)*2**submode
        phase=modulo(phase+twopi*freq/12000.0_real64,twopi)
        value=value+amplitude*cos(phase)
     endif
     samples(i)=int(nint(max(-32768.0_real64,min(32767.0_real64,value))),c_int16_t)
  enddo
end subroutine q65_engine_make_signal
