module sfrx_engine
  use iso_c_binding, only: c_ptr
  use packjt77, only: pack77_state
  use sfox_unpack_module, only: superfox_callback, sfox_unpack_for_state, sfox_render_legacy
  use sfox_remove_ft8_module, only: sfox_remove_ft8_for_state
  implicit none
  private
  public :: sfrx_decode, superfox_callback
contains
subroutine sfrx_decode(knowledge,callback,user_context,nyymmdd,nutc,nfqso,ntol,iwave)

  use sfox_mod
  use julian
  implicit real(a-h,o-z)
  implicit integer(i-n)
  type(pack77_state), optional, intent(inout) :: knowledge
  procedure(superfox_callback) :: callback
  type(c_ptr), intent(in) :: user_context

  integer*2 iwave(NMAX)
  integer*8 secday,ntime8
  integer*1 xdec(0:49)
  character*13 foxcall
  complex, allocatable :: c0(:)       !Complex form of signal as received
  real, allocatable :: dd(:)
  logical crc_ok
  data secday/86400/

  fsync=nfqso
  ftol=ntol
  fsample=12000.0
  call sfox_init(7,127,50,'no',fspread,delay,fsample,24)
  npts=15*12000

  if(nyymmdd.eq.-1) then
     ntime8=itime8()/30
     ntime8=30*ntime8
  else
     iyr=2000+nyymmdd/10000
     imo=mod(nyymmdd/100,100)
     iday=mod(nyymmdd,100)
     ih=nutc/10000
     im=mod(nutc/100,100)
     is=mod(nutc,100)
     ntime8=secday*(JD(iyr,imo,iday)-2440588) + 3600*ih + 60*im + is
  endif

  allocate(c0(NMAX),dd(NMAX))
  dd=iwave
  call sfox_remove_ft8_for_state(knowledge,dd,npts)

  call sfox_ana(dd,npts,c0,npts)

  call sfox_remove_tone(c0,fsync)  ! Needs testing

  ndepth=3
  dth=0.5
  damp=1.0

  call qpc_decode2(c0,fsync,ftol, xdec,ndepth,dth,damp,crc_ok,   &
       snrsync,fbest,tbest,snr)
  if(crc_ok) then
     nsnr=nint(snr)
     nsignature = 1
     call sfox_unpack_for_state(knowledge,callback,user_context,nutc,xdec,nsnr,fbest-750.0,tbest,foxcall,nsignature)
  endif

  return
end subroutine sfrx_decode
end module sfrx_engine

subroutine sfrx_sub(nyymmdd,nutc,nfqso,ntol,iwave)
  use iso_c_binding, only: c_null_ptr
  use sfrx_engine, only: sfrx_decode
  use sfox_unpack_module, only: sfox_render_legacy
  use sfox_mod, only: NMAX
  implicit none
  integer nyymmdd,nutc,nfqso,ntol
  integer*2 iwave(NMAX)

  call sfrx_decode(callback=sfox_render_legacy,user_context=c_null_ptr,nyymmdd=nyymmdd,nutc=nutc, &
       nfqso=nfqso,ntol=ntol,iwave=iwave)
end subroutine sfrx_sub
