module jt9_soft_symbols
  use jt9_downsample, only: jt9_downsample_workspace, downsam9
  implicit none
  private
  public :: softsym
contains

subroutine softsym(workspace,id2,npts8,nsps8,fpk,syncpk,snrdb,xdt,        &
     freq,drift,a3,schk,i1SoftSymbols)

! Compute the soft symbols

  use timer_module, only: timer

  implicit real(a-h,o-z), integer(i-n)
  parameter (NZ2=1512,NZ3=1360)
  type(jt9_downsample_workspace), intent(inout) :: workspace
  integer*2, intent(in) :: id2(*)
  complex c2(0:NZ2-1)
  complex c3(0:NZ3-1)
  complex c5(0:NZ3-1)
  real a(3)
  integer*1 i1SoftSymbolsScrambled(207)
  integer*1 i1SoftSymbols(207)
  include 'jt9sync.f90'

  nspsd=16
  ndown=nsps8/nspsd

! Mix, low-pass filter, and downsample to 16 samples per symbol
  call timer('downsam9',0)
  call downsam9(workspace,id2,npts8,fpk,c2)
  call timer('downsam9',1)
  if(all(c2.eq.(0.,0.))) then
     syncpk=-1.
     snrdb=-99.
     xdt=0.
     freq=fpk
     drift=0.
     a3=0.
     schk=0.
     i1SoftSymbols=0
     return
  endif

  call peakdt9(c2,nsps8,nspsd,c3,xdt)  !Find DT

  fsample=1500.0/ndown
  a=0.
  call timer('afc9    ',0)
  call afc9(c3,nz3,fsample,a,syncpk)  !Find deltaF, fDot, extra DT
  call timer('afc9    ',1)
  freq=fpk - a(1)
  drift=-2.0*a(2)
!  write(*,3301) fpk,freq,a
!3301 format(2f9.3,3f10.4)
  a3=a(3)
  a(3)=0.

  call timer('twkfreq ',0)
  call twkfreq(c3,c5,nz3,fsample,a)   !Correct for delta f, f1, f2 ==> a(1:3)
  call timer('twkfreq ',1)

! Compute soft symbols (in scrambled order)
  call timer('symspec2',0)
  call symspec2(c5,nz3,nsps8,nspsd,fsample,freq,drift,snrdb,schk,      &
       i1SoftSymbolsScrambled)
  call timer('symspec2',1)

! Remove interleaving
  call interleave9(i1SoftSymbolsScrambled,-1,i1SoftSymbols)

  return
end subroutine softsym

end module jt9_soft_symbols
