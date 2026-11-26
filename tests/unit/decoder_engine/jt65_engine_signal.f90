subroutine decoder_engine_test_jt65_signal(submode,kind,seed,samples) bind(C)
  use iso_c_binding, only: c_int,c_int16_t
  use iso_fortran_env, only: real32,real64
  use jt65_test_fixture, only: make_jt65_wave,jt65_sample_count
  use jt65_test_vectors, only: standard_tones,ooo_tones
  implicit none
  integer(c_int), value :: submode,kind,seed
  integer(c_int16_t), intent(out) :: samples(jt65_sample_count)
  real(real32), allocatable :: wave(:), second(:)
  integer :: tones(126),i,j,itype
  character :: message(23),sent(23)
  character(len=22) :: text

  interface
     subroutine gen65(message,check,sent,tones,message_type) bind(C)
       import c_int
       character :: message(23),sent(23)
       integer(c_int) :: check,tones(126),message_type
     end subroutine
  end interface

  allocate(wave(jt65_sample_count))
  tones=standard_tones
  if(kind==1) tones=ooo_tones
  if(kind>=3.and.kind<=5) then
     tones=0
     do j=1,126
        if(mod((j-1)/4,2)==1) tones(j)=10*(kind-1)
     enddo
  endif
  if(kind==6) then
     call make_jt65_wave(wave,tones,1,1500.0_real64,600.0_real64,2500.0_real64,seed)
  elseif(kind==2) then
     call make_jt65_wave(wave,tones,2**submode,1500.0_real64,110.0_real64,2500.0_real64,seed)
  else
     call make_jt65_wave(wave,tones,2**submode,1500.0_real64,300.0_real64,100.0_real64,seed)
  endif
  if(kind==6) then
     allocate(second(jt65_sample_count))
     text="K2DEF N0XYZ EM00"
     message=" "
     do i=1,22
        message(i)=text(i:i)
     enddo
     call gen65(message,0,sent,tones,itype)
     call make_jt65_wave(second,tones,1,2100.0_real64,500.0_real64)
     wave=wave+second
  endif
  do i=1,size(samples)
     samples(i)=int(nint(max(-32768.0_real32,min(32767.0_real32,wave(i)))),c_int16_t)
  enddo
end subroutine decoder_engine_test_jt65_signal
