subroutine decode9w(nfqso,ntol,nsubmode,ss,nzhsym,id2,sync,nsnr,xdt1,f0,decoded)

! Decode a weak signal in a wide/slow JT9 submode.

  parameter (NSMAX=6827,NZMAX=60*12000)
  real ss(184,NSMAX)                     !Symbol spectra at 1/2-symbol steps
  real ccfred(NSMAX)                     !Best sync vs frequency
  real ccfblue(-9:18)                    !Sync vs time at best frequency
  real a(5)                              !Fitted Lorentzian params
  integer*2 id2(NZMAX)                   !Raw 16-bit data
  integer*1 i1SoftSymbols(207)           !Binary soft symbols
  logical :: valid,narrow
  character*22 decoded                   !Decoded message

  decoded=' '
  sync=0.
  nsnr=-99
  xdt1=0.
  f0=nfqso

  df=12000.0/16384.0                     !Bin spacing in ss()
  nsps=6912                              !Samples per 9-FSK symbol
  tstep=nsps*0.5/12000.0                 !Half-symbol duration
  npts=52*12000
  limit=10000                            !Fano timeout parameter

  ia=max(1,nint((nfqso-ntol)/df)+1)      !Start frequency bin
  ib=min(NSMAX,nint((nfqso+ntol)/df)+1)  !End frequency bin
  if(ib.lt.ia) return
  narrow=ib-ia+1.lt.20
  ifita=ia
  ifitb=ib
  if(narrow) then
     ! Fit both 20-bin baseline flanks without widening candidate selection.
     ifita=max(1,min(NSMAX-40,nint(nfqso/df)+1-20))
     ifitb=ifita+40
  endif
  lag1=-int(2.5/tstep + 0.9999)          !Start lag
  lag2=int(5.0/tstep + 0.9999)           !End lag
  nhsym=max(0,min(184,nzhsym))
  if(nhsym.eq.0) return

! First sync pass finds approximate Doppler spread; second pass does a
! good Lorentzian fit to determine frequency f0.
  do iter=1,2
     nadd=3
     if(iter.eq.2) nadd=2*nint(0.375*a(4)) + 1
     call sync9w(ss,nhsym,lag1,lag2,ifita,ifitb,ia,ib,ccfred,ccfblue,ipk,lagpk,nadd)
     if(ipk.eq.0) return
     s=0.
     sq=0.
     ns=0
     do i=-9,18
        if(abs(i-lagpk).gt.3) then
           s=s+ccfblue(i)
           sq=sq+ccfblue(i)**2
           ns=ns+1
        endif
     enddo
     base=s/ns
     rms=sqrt(max(0.,sq/ns - base**2))
     if(rms.le.0.) return
     sync=(ccfblue(lagpk)-base)/rms
     xdt0=lagpk*tstep
     call lorentzian(ccfred(ifita),ifitb-ifita+1,a)
     f0=(ifita+a(3)-2)*df
  enddo
  if(narrow .and. abs(f0-nfqso).gt.ntol) return
  ccfblue=(ccfblue-base)/rms

  call softsym9w(id2,npts,xdt0,f0,a(4)*df,nsubmode,xdt1,snrdb,i1softsymbols,valid)
  if(.not.valid) return
  xdt1=xdt1-1.05
  nsnr=nint(snrdb)
  call jt9fano(i1softsymbols,limit,nlim,decoded)

!###
!  do i=-9,18
!     write(81,3081) i,ccfblue(i)
!3081 format(i3,f10.3)
!  enddo
!  do i=1,NSMAX
!     write(82,3082) i*df,ccfred(i)
!3082 format(f10.1,e12.3)
!  enddo
!###

  return
end subroutine decode9w
