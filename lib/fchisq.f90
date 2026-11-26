real function fchisq(c3,npts,fsample,a)

  parameter (NMAX=85*16)
  complex c3(npts)
  complex c4(NMAX)
  real a(3)
  complex z
  include 'jt9sync.f90'

  call twkfreq(c3,c4,npts,fsample,a)

! Get sync power.
  nspsd=16
  sum1=0.
  k=-1
  do i=1,85
     z=0.
     do j=1,nspsd
        k=k+1
        z=z+c4(k+1)
     enddo
     pp=real(z)**2 + aimag(z)**2     
     if(isync(i).eq.1) then
        sum1=sum1+pp
     endif
  enddo
  fchisq=-sum1/10000.0

  return
end function fchisq
