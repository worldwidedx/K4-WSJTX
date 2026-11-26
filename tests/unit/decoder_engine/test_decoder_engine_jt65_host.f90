program test_decoder_engine_jt65_host
  use iso_c_binding, only: c_int,c_int8_t,c_int16_t,c_int64_t,c_sizeof
  use decoder_engine, only: engine_host_decode
  use ft8_engine_kernel, only: params_block
  use decode_completion_module, only: decode_completion_result
  use prog_args, only: data_dir,temp_dir
  implicit none
  type(params_block) :: params,before_params
  type(decode_completion_result) :: completion
  integer(c_int8_t) :: zeros(c_sizeof(params))
  integer(c_int16_t), allocatable, target :: samples(:)
  integer(c_int16_t), allocatable :: before_samples(:)
  integer(c_int64_t) :: input_id=1000,analysis_id=1000
  integer :: unit,attempt=1
  interface
     subroutine make_signal(submode,kind,seed,samples) bind(C,name='decoder_engine_test_jt65_signal')
       import c_int,c_int16_t
       integer(c_int), value :: submode,kind,seed
       integer(c_int16_t), intent(out) :: samples(720000)
     end subroutine
  end interface
  allocate(samples(720000),before_samples(720000))
  data_dir='.'
  temp_dir='.'
  zeros=0
  params=transfer(zeros,params)
  params%nmode=65
  params%ntr=60
  params%ndiskdat=.true.
  params%newdat=.true.
  params%nfqso=1500
  params%nfa=1400
  params%nfb=1600
  params%ntol=100
  params%n2pass=1
  params%nranera=6
  params%naggressive=5
  params%nQSOProgress=5
  params%mycall=transfer('K1ABC       ',params%mycall)
  params%hiscall=transfer('W9XYZ       ',params%hiscall)
  params%hisgrid=transfer('FN42  ',params%hisgrid)
  params%nexp_decode=32+64
  params%ndepth=1+16
  params%nclearave=.true.

  call reception(1)
  call run(0,'first weak reception')
  call averages(1,1,0)
  params%nclearave=.false.
  params%nagain=.true.
  call run(0,'repeat does not add an average')
  call averages(1,1,0)
  params%nclearave=.true.
  call run(0,'clear and repeat add only the current reception')
  call averages(1,1,0)
  params%nclearave=.false.

  call reception(2)
  call run(0,'opposite parity does not recover')
  call averages(2,0,1)
  call reception(3)
  call run(1,'averaging packed flag recovers message')
  call output_has('K1ABC W9XYZ FN42',1,2,.true.)
  call averages(3,2,0)
  params%nclearave=.true.
  params%ndepth=1
  call reception(5)
  call run(0,'averaging disabled control')
  call averages(0,0,0)
  params%nclearave=.false.

  params%ljt65apon=.true.
  params%nQSOProgress=1
  call reception(3)
  call run(1,'AP flag recovers weak message')
  call output_has('K1ABC W9XYZ FN42',-1,1,.true.)
  params%ljt65apon=.false.
  call reception(3)
  call run(0,'AP disabled control')
  call output_has('K1ABC W9XYZ FN42',0,0,.false.)

  params%nQSOProgress=5
  params%hiscall=' '
  params%hisgrid=' '
  open(newunit=unit,file='CALL3.TXT',status='replace')
  write(unit,'(a)') 'W9XYZ,FN42,,'
  close(unit)
  params%ndepth=1+32
  call reception(1)
  call run(1,'deep-search packed flag loads caller file')
  call output_has('K1ABC W9XYZ FN42',2,1,.true.)
  params%ndepth=1
  call reception(1)
  call run(0,'deep search disabled control')
  params%ndepth=1+32
  open(newunit=unit,file='CALL3.TXT',status='replace')
  write(unit,'(a)') 'N0CALL,EM00,,'
  close(unit)
  call reception(1)
  call run(0,'different caller file does not recover target')
  deallocate(samples,before_samples)
  print '(a)', 'JT65 host adapter tests passed'
contains
  subroutine require(condition,description)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: description
    if(.not.condition) then
       print '(a)', 'FAIL: '//description
       error stop 1
    endif
  end subroutine

  subroutine reception(utc)
    integer, intent(in) :: utc
    input_id=input_id+1
    params%nutc=utc
    params%nagain=.false.
    attempt=0
    call make_signal(0,2,utc,samples)
  end subroutine

  subroutine run(messages,description)
    integer, intent(in) :: messages
    character(len=*), intent(in) :: description
    ! The compatibility writer appends repeats; isolate each attempt's output.
    open(newunit=unit,file='decoded.txt',status='replace')
    close(unit)
    before_params=params
    before_samples=samples
    attempt=attempt+1
    analysis_id=analysis_id+1
    call engine_host_decode(samples,params,12000,completion,0,input_id,analysis_id,attempt,size(samples))
    call require(completion%available,description//' completion available')
    call require(completion%decoded==messages,description//' decoded count')
    call require(all(samples==before_samples),description//' immutable PCM')
    call require(all(transfer(params,zeros)==transfer(before_params,zeros)),description//' immutable options')
  end subroutine

  subroutine output_has(message,method,periods,expected)
    character(len=*), intent(in) :: message
    integer, intent(in) :: method,periods
    logical, intent(in) :: expected
    character(len=256) :: line
    integer :: ios,matches,ft,nsum,nsmo,start
    matches=0
    open(newunit=unit,file='decoded.txt',status='old',action='read')
    do
       read(unit,'(a)',iostat=ios) line
       if(ios/=0) exit
       if(index(line,message)>0) then
          matches=matches+1
          start=index(line,' JT65')
          call require(start>0,'legacy result mode')
          read(line(start+5:),*) ft,nsum,nsmo
          if(method<0) then
             call require(ft>3,'legacy AP qualifier')
          else
             call require(ft==method,'legacy decoding method')
          endif
          call require(nsum==periods,'legacy averaging qualifier')
       endif
    enddo
    close(unit)
    call require(matches==merge(1,0,expected),'current attempt decoded output')
  end subroutine

  subroutine averages(expected,odd_used,even_used)
    integer, intent(in) :: expected,odd_used,even_used
    character(len=100) :: line
    integer :: ios,count,utc,odd,even
    count=0
    odd=0
    even=0
    open(newunit=unit,file='avemsg.txt',status='old',action='read')
    do
       read(unit,'(a)',iostat=ios) line
       if(ios/=0) exit
       count=count+1
       read(line(2:6),'(i5)') utc
       call require(utc==count,'legacy average reception UTC')
       if(line(1:1)=='$') then
          if(mod(utc,2)==1) odd=odd+1
          if(mod(utc,2)==0) even=even+1
       endif
    enddo
    close(unit)
    call require(count==expected,'legacy average history count')
    call require(odd==odd_used.and.even==even_used,'legacy selected averaging periods')
  end subroutine
end program test_decoder_engine_jt65_host
