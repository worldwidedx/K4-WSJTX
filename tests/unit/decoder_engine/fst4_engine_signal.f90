module fst4_engine_workspace_test
  use iso_fortran_env, only: error_unit
  use fst4_decode, only: fst4_decoder
  use decoder_engine_types, only: fst4_result
  implicit none
  private
  public :: workspace_test_decoder,require,observe

  type, extends(fst4_decoder) :: workspace_test_decoder
     integer :: observations=0
     logical :: expected_doppler=.false.
  end type

contains
  subroutine require(condition,description)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: description
    if(condition) return
    write(error_unit,'(a)') 'FST4 workspace: '//description
    error stop 1
  end subroutine

  subroutine observe(this,nutc,sync,nsnr,dt,freq,decoded,nap,qual,ntrperiod,fmid,w50,result)
    class(fst4_decoder), intent(inout) :: this
    integer, intent(in) :: nutc,nsnr,nap,ntrperiod
    real, intent(in) :: sync,dt,freq,qual,fmid,w50
    character(len=37), intent(in) :: decoded
    type(fst4_result), optional, intent(in) :: result
    select type(this)
    type is(workspace_test_decoder)
       call require(decoded=='K1ABC W9XYZ FN42','correct message after workspace reacquisition')
       call require(present(result),'typed result after workspace reacquisition')
       call require(result%has_doppler==merge(1,0,this%expected_doppler),'requested Doppler analysis')
       this%observations=this%observations+1
    class default
       error stop 'invalid FST4 workspace test decoder'
    end select
  end subroutine
end module fst4_engine_workspace_test

subroutine decoder_engine_test_fst4_signal(mode,period,frequency,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t,c_float
  implicit none
  integer(c_int), value :: mode,period
  real(c_float), value :: frequency
  integer(c_int16_t), intent(out) :: samples(period*12000)
  call fst4_engine_make_signal(mode,period,frequency,300.0,0,-1,samples)
end subroutine

subroutine decoder_engine_test_fst4_weak_signal(period,amplitude,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t,c_float
  implicit none
  integer(c_int), value :: period
  real(c_float), value :: amplitude
  integer(c_int16_t), intent(out) :: samples(period*12000)
  call fst4_engine_make_signal(241,period,1500.0,amplitude,0,-1,samples)
end subroutine

subroutine decoder_engine_test_fst4_ap_signal(samples) bind(C)
  use iso_c_binding, only: c_int16_t,c_float
  implicit none
  integer(c_int16_t), intent(out) :: samples(15*12000)
  call fst4_engine_make_signal(240,15,1500.0,300.0,1,-1,samples)
end subroutine

subroutine decoder_engine_test_fst4_rr73_signal(samples) bind(C)
  use iso_c_binding, only: c_int16_t
  implicit none
  integer(c_int16_t), intent(out) :: samples(15*12000)
  call fst4_engine_make_signal(240,15,1500.0,300.0,2,-1,samples)
end subroutine

subroutine decoder_engine_test_fst4_hash_signals(count,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t
  implicit none
  integer(c_int), value :: count
  integer(c_int16_t), intent(out) :: samples(15*12000)
  integer(c_int16_t), allocatable :: second(:)
  if(count<1.or.count>2) error stop 'invalid FST4 hash fixture count'
  call fst4_engine_make_signal(241,15,merge(1500.0,1450.0,count==1),300.0,0,1234567,samples)
  if(count==2) then
     allocate(second(size(samples)))
     call fst4_engine_make_signal(241,15,1550.0,300.0,0,2345678,second)
     samples=int(max(-32768,min(32767,int(samples)+int(second))),c_int16_t)
  endif
end subroutine

subroutine fst4_engine_make_signal(mode,period,frequency,amplitude,payload_damage,hash22,samples)
  use iso_c_binding, only: c_int,c_int16_t,c_float
  use iso_fortran_env, only: int64,real64
  use packjt77, only: pack77_state,pack77_for_state,pack77_options,PACK77_STATUS_ENCODED
  use packjt77_schema, only: encode_pack77_wspr_type3
  use packjt77_grammar, only: pack77_grid6_wspr_index
  implicit none
  integer(c_int), intent(in) :: mode,period,hash22,payload_damage
  real(c_float), intent(in) :: frequency,amplitude
  integer(c_int16_t), intent(out) :: samples(period*12000)
  logical :: encoded
  type(pack77_state), target :: knowledge
  real, allocatable :: wave(:)
  complex :: unused_complex(1)
  integer :: tones(160),nsps,start,i,j,i3,n3,status,iwspr,ncrc,length
  integer(c_int16_t) :: sample
  integer(kind=1) :: bits(101)
  integer, parameter :: scramble(77)=[0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0, &
       1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1, &
       0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1]
  integer(int64) :: rng
  real(real64) :: noise,u1,u2,value
  real(real64), parameter :: twopi=6.283185307179586476925286766559_real64
  character(len=37) :: message
  character(len=77) :: packed
  character(len=24) :: crc

  select case(period)
  case(15)
     nsps=720
  case(30)
     nsps=1680
  case(60)
     nsps=3888
  case(120)
     nsps=8200
  case(300)
     nsps=21504
  case(900)
     nsps=66560
  case(1800)
     nsps=134400
  case default
     error stop 'invalid FST4 fixture period'
  end select
  iwspr=merge(1,0,mode==241)
  message='K1ABC W9XYZ FN42'
  if(payload_damage==2) message='K1ABC W9XYZ RR73'
  if(iwspr==1) message='K1ABC FN42 37'
  if(hash22>=0) then
     call encode_pack77_wspr_type3(hash22,pack77_grid6_wspr_index('FN42AB'),packed,encoded)
     if(.not.encoded) error stop 'FST4 hash fixture pack failed'
  else
     call pack77_for_state(knowledge,message,i3,n3,packed, &
          pack77_options(prefer_wspr_50bit=iwspr==1),status)
     if(status/=PACK77_STATUS_ENCODED) error stop 'FST4 fixture pack failed'
  endif
  bits=0
  length=77
  if(iwspr==1) length=50
  read(packed,'(77i1)') bits(1:77)
  if(iwspr==0) bits(1:77)=mod(bits(1:77)+scramble,2)
  bits(length+1:)=0
  call get_crc24(bits,length+24,ncrc)
  write(crc,'(b24.24)') ncrc
  read(crc,'(24i1)') bits(length+1:length+24)
  call get_fst4_tones_from_bits(bits,tones,iwspr)
  if(payload_damage==1) tones(9:37)=mod(tones(9:37)+2,4)
  if(payload_damage==2) then
     ! Damage beyond the callsign bits makes recovery depend on the full RR73 AP hypothesis.
     tones(9:38)=mod(tones(9:38)+2,4)
     tones(47:70)=mod(tones(47:70)+2,4)
  endif
  allocate(wave(period*12000))
  call gen_fst4wave(tones,160,nsps,size(wave),12000.0,1, &
       frequency+1.5*12000.0/nsps,0,unused_complex,wave)
  start=12000
  if(period==15) start=6000
  rng=104729_int64
  do i=1,size(samples)
     rng=modulo(16807_int64*rng,2147483647_int64)
     u1=max(real(rng,real64)/2147483647.0_real64,tiny(1.0_real64))
     rng=modulo(16807_int64*rng,2147483647_int64)
     u2=real(rng,real64)/2147483647.0_real64
     noise=100*sqrt(-2*log(u1))*cos(twopi*u2)
     value=noise
     j=i-start
     if(j>0.and.j<=160*nsps) value=value+amplitude*wave(j)
     sample=int(nint(max(-32768.0_real64,min(32767.0_real64,value))),c_int16_t)
     samples(i)=sample
  enddo
  samples(1)=-32767_c_int16_t-1_c_int16_t
end subroutine fst4_engine_make_signal

subroutine decoder_engine_test_fst4_workspace() bind(C)
  use iso_c_binding, only: c_int16_t,c_ptr,c_null_ptr,c_associated,c_loc
  use fst4_decode, only: fst4_options
  use fst4_engine_workspace_test, only: workspace_test_decoder,require,observe
  use fftw3, only: fftwf_destroy_plan,FFTW_MEASURE
  implicit none
  type(workspace_test_decoder) :: decoder
  type(fst4_options) :: options
  type(c_ptr) :: big_plan,baseband_plan,dop_plan,baseband,frame
  integer(c_int16_t), allocatable :: samples(:),saved_pcm(:)
  integer :: attempt,status

  options%period=300
  options%low_frequency=1400
  options%high_frequency=1600
  options%receive_frequency=1500
  options%tolerance=50
  options%single_decode=.true.
  options%measure_doppler=.true.
  allocate(samples(options%period*12000))
  call fst4_engine_make_signal(240,options%period,1500.0,300.0,0,-1,samples)
  do attempt=1,4
     if(attempt==3) then
        call decoder%reset()
        call require(c_associated(decoder%big_plan,big_plan).and. &
             c_associated(decoder%baseband_plan,baseband_plan).and.c_associated(decoder%dop_plan,dop_plan), &
             'reset retains FFT plans before the next decode')
     endif
     if(attempt==4) then
        call fftwf_destroy_plan(decoder%big_plan)
        call fftwf_destroy_plan(decoder%baseband_plan)
        call fftwf_destroy_plan(decoder%dop_plan)
        decoder%big_plan=c_null_ptr
        decoder%baseband_plan=c_null_ptr
        decoder%dop_plan=c_null_ptr
     endif
     decoder%observations=0
     decoder%expected_doppler=options%measure_doppler
     call decoder%decode_pcm(observe,samples,size(samples),options,status)
     call require(status==0.and.decoder%observations==1,'one result after workspace reacquisition')
     call require(.not.associated(decoder%big),'release long-period FFT scratch')
     call require(.not.associated(decoder%dopgain).and..not.associated(decoder%dopwave), &
          'release long-period Doppler scratch')
     call require(.not.allocated(decoder%candidate_s).and..not.allocated(decoder%candidate_s2).and. &
          .not.allocated(decoder%candidate_base).and..not.allocated(decoder%baseline_scratch), &
          'release long-period candidate scratch')
     call require(c_associated(decoder%big_plan).and.c_associated(decoder%baseband_plan).and. &
          c_associated(decoder%dop_plan),'retain valid FFT plans')
     call require(associated(decoder%baseband).and.associated(decoder%frame),'retain smaller work buffers')
     if(attempt==1) then
        big_plan=decoder%big_plan
        baseband_plan=decoder%baseband_plan
        dop_plan=decoder%dop_plan
        baseband=c_loc(decoder%baseband(1))
        frame=c_loc(decoder%frame(1))
     else
        if(attempt<4) call require(c_associated(decoder%big_plan,big_plan).and. &
             c_associated(decoder%baseband_plan,baseband_plan).and.c_associated(decoder%dop_plan,dop_plan), &
             'repeat and reset retain the same FFT plans')
        call require(c_associated(c_loc(decoder%baseband(1)),baseband).and. &
             c_associated(c_loc(decoder%frame(1)),frame),'repeat and reset retain smaller allocations')
     endif
  enddo
  options%period=15
  call fst4_engine_make_signal(240,options%period,1500.0,300.0,0,-1,samples(1:15*12000))
  decoder%observations=0
  decoder%expected_doppler=options%measure_doppler
  call decoder%decode_pcm(observe,samples,15*12000,options,status)
  call require(status==0.and.decoder%observations==1,'short-period decode after long-period scratch release')
  call require(associated(decoder%big).and.associated(decoder%dopgain).and.associated(decoder%dopwave), &
       'retain short-period FFT and Doppler scratch')
  call require(allocated(decoder%candidate_s).and.allocated(decoder%candidate_s2).and. &
       allocated(decoder%candidate_base).and.allocated(decoder%baseline_scratch),'retain short-period candidate scratch')
  allocate(saved_pcm(15*12000))
  saved_pcm=samples(1:15*12000)
  options%fft_flags=FFTW_MEASURE
  options%measure_doppler=.false.
  decoder%observations=0
  decoder%expected_doppler=options%measure_doppler
  call decoder%decode_pcm(observe,samples,15*12000,options,status)
  call require(status==0.and.decoder%observations==1,'decode after changing FFT planning effort')
  call require(all(samples(1:15*12000)==saved_pcm),'MEASURE planning preserves caller PCM')
  call require(c_associated(decoder%big_plan).and.c_associated(decoder%baseband_plan), &
       'MEASURE creates the required FFT plans')
  call require(.not.c_associated(decoder%dop_plan),'planning effort invalidates the unused Doppler plan')
  options%measure_doppler=.true.
  do attempt=1,2
     decoder%observations=0
     decoder%expected_doppler=options%measure_doppler
     call decoder%decode_pcm(observe,samples,15*12000,options,status)
     call require(status==0.and.decoder%observations==1,'decode with MEASURE Doppler planning')
     call require(all(samples(1:15*12000)==saved_pcm),'Doppler MEASURE planning preserves caller PCM')
     call require(c_associated(decoder%dop_plan),'recreate the Doppler plan at the selected planning effort')
     if(attempt==1) then
        big_plan=decoder%big_plan
        baseband_plan=decoder%baseband_plan
        dop_plan=decoder%dop_plan
     else
        call require(c_associated(decoder%big_plan,big_plan).and. &
             c_associated(decoder%baseband_plan,baseband_plan).and.c_associated(decoder%dop_plan,dop_plan), &
             'unchanged planning effort reuses every FFT plan')
     endif
  enddo
  call decoder%destroy()
end subroutine decoder_engine_test_fst4_workspace
