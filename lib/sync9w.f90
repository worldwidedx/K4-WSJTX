subroutine sync9w(ss,nzhsym,lag1,lag2,ia,ib,icandidatea,icandidateb,ccfred,ccfblue,ipkbest,lagpk,nadd)

  include 'constants.f90'
  real ss(184,NSMAX)
  real ss1(184),ss1save(184)
  real ccfred(NSMAX)
  real ccfblue(-9:18)
  real sa(NSMAX),sb(NSMAX)
  include 'jt9sync.f90'

! Smooth the symbol spectra (by an amount consistent with measured width??)
  do j=1,nzhsym
     sa=ss(j,1:NSMAX)
     call smo(sa,NSMAX,sb,nadd)
     call smo(sa,NSMAX,sb,nadd)
     ss(j,1:NSMAX)=sa
  enddo

  lagpk=0
  ccfblue=0.
  ipk=0
  ipkbest=0
  sbest=0.
  ccfred=0.
  df=12000.0/16384.0

  do i=ia,ib                                 !Loop over specified freq range
     ss1=0.
     ss1(1:nzhsym)=ss(1:nzhsym,i)           !Available symbol amplitudes at this freq
     call pctile(ss1,nzhsym,50,xmed)         !Median level at this freq
     if(xmed.le.0.) cycle
     ss1(1:nzhsym)=ss1(1:nzhsym)/xmed - 1.0

     smax=0.                                 !Find DT in specified range
     do lag=lag1,lag2
        sum1=0.
        nsum=nzhsym
        do j=1,16                            !Sum over 16 sync symbols
           k=ii2(j) + lag
           if(k.ge.1 .and. k.le.nzhsym) then
              sum1=sum1 + ss1(k)
              nsum=nsum-1
           endif
        enddo
        if(sum1.gt.smax) then
           smax=sum1
           ipk=i 
        endif
     enddo

     ccfred(i)=smax                        !Best at this freq, over all lags
     if(i.ge.icandidatea .and. i.le.icandidateb .and. smax.gt.sbest) then
        sbest=smax
        ipkbest=ipk
        ss1save=ss1
     endif
  enddo

  if(ipkbest.eq.0) return
  call pctile(ccfred(ia),ib-ia+1,50,xmed)
  if(xmed.le.0.0) xmed=1.0
  ccfred=ccfred/xmed

  ss1=ss1save
  smax=0.                                 !Find DT in specified range
  do lag=lag1,lag2
     sum1=0.
     nsum=nzhsym
     do j=1,16                            !Sum over 16 sync symbols
        k=ii2(j) + lag
        if(k.ge.1 .and. k.le.nzhsym) then
           sum1=sum1 + ss1(k)
           nsum=nsum-1
        endif
     enddo
     ccfblue(lag)=sum1
     if(sum1.gt.smax) then
        smax=sum1
        lagpk=lag
     endif
  enddo
  if(lagpk.eq.-9) lagpk=-8                !Protect the ends of ccfblue()
  if(lagpk.eq.18) lagpk=17

  return
end subroutine sync9w
