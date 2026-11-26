subroutine sync65(nfa,nfb,ntol,nqsym,ca,ncand,nrobust,bVHF)

  use jt65_mod, only: jt65_work,ensure_jt65_workspace

  parameter (NSZ=3413,NFFT=8192,MAXCAND=300)
  real ccfblue(-32:82)             !CCF with pseudorandom sequence
  real ccfred(NSZ)                  !Peak of ccfblue, as function of freq
  logical bVHF

  type candidate
     real freq
     real dt
     real sync
     real flip
  end type candidate
  type(candidate) ca(MAXCAND)

  common/steve/thresh0

  call ensure_jt65_workspace()
  if(.not.allocated(jt65_work%spectra)) allocate(jt65_work%spectra(552,3413))
  associate(ss=>jt65_work%spectra)
  if(ntol.eq.-99) stop                       !Silence compiler warning
  call setup65

  df=12000.0/NFFT                            !df = 12000.0/8192 = 1.465 Hz
  ia=max(2,nint(nfa/df))
  ib=min(NSZ-1,nint(nfb/df))
  ncand=0
  if(ib<ia) return
!  lag1=-11
!  lag2=59
!  lag1=-22
!  lag2=118
  lag1=-32
  lag2=82        !may need to be extended for EME
  nsym=126
  ncand=0
  fdot=0.
  ccfred=0.
  ccfblue=0.
  ccfmax=0.
  ipk=0
  do i=ia,ib
     call xcor(i,nqsym,nsym,lag1,lag2,ccfblue,ccf0,lagpk0,flip,fdot,nrobust)
! Remove best-fit slope from ccfblue and normalize so baseline rms=1.0
     if(.not.bVHF) call slope(ccfblue(lag1),lag2-lag1+1,      &
          lagpk0-lag1+1.0)
     ccfred(i)=ccfblue(lagpk0)
     if(ccfred(i).gt.ccfmax) then
        ccfmax=ccfred(i)
        ipk=i
     endif
  enddo
  call pctile(ccfred(ia:ib),ib-ia+1,35,xmed)
  ccfred(ia:ib)=ccfred(ia:ib)-xmed
  ccfred(ia-1)=ccfred(ia)
  ccfred(ib+1)=ccfred(ib)

  do i=ia,ib
     freq=i*df
     itry=0
     if(bVHF) then
        if(i.ne.ipk .or. ccfmax.lt.thresh0) cycle
        itry=1
        ncand=ncand+1
     else
        if(ccfred(i).ge.thresh0 .and. ccfred(i).gt.ccfred(i-1) .and.       &
             ccfred(i).gt.ccfred(i+1)) then
           itry=1
           ncand=ncand+1
        endif
     endif
     if(itry.ne.0) then
        call xcor(i,nqsym,nsym,lag1,lag2,ccfblue,ccf0,lagpk,flip,fdot,nrobust)
        if(.not.bVHF) call slope(ccfblue(lag1),lag2-lag1+1,       &
             lagpk-lag1+1.0)
        xlag=lagpk
        if(lagpk.gt.lag1 .and. lagpk.lt.lag2) then
           call peakup(ccfblue(lagpk-1),ccfmax,ccfblue(lagpk+1),dx2)
           xlag=lagpk+dx2
        endif
        dtx=xlag*1024.0/11025.0
        ccfblue(lag1)=0.
        ccfblue(lag2)=0.
        ca(ncand)%freq=freq
        ca(ncand)%dt=dtx
        ca(ncand)%flip=flip
        if(bVHF) then
           ca(ncand)%sync=db(ccfred(i)) - 16.0
        else
           ca(ncand)%sync=ccfred(i)
        endif
     endif
     if(ncand.eq.MAXCAND) exit
  enddo

  end associate
  return
end subroutine sync65
