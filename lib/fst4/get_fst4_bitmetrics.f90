module fst4_bitmetrics
   implicit none
   type fst4_bitmetrics_workspace
      complex, allocatable :: ci(:,:)
      logical, allocatable :: one(:,:),zero(:,:)
      real, allocatable :: s2(:)
      integer :: nss=0
   end type
contains
subroutine get_fst4_bitmetrics_owned(work,cd,nss,bitmetrics,s4,nsync_qual,badsync)

   use timer_module, only: timer
   implicit real(a-h,o-z)
   implicit integer(i-n)
   include 'fst4_params.f90'
   complex cd(0:NN*nss-1)
   complex cs(0:3,NN)
   type(fst4_bitmetrics_workspace), intent(inout), target :: work
   complex c1(4,8),c2(16,4),c4(256,2)
   integer isyncword1(0:7),isyncword2(0:7)
   integer graymap(0:3)
   integer ip(1)
   integer hbits(2*NN)
   logical badsync
   real bitmetrics(2*NN,4)
   real s4(0:3,NN)
   data isyncword1/0,1,3,2,1,0,2,3/
   data isyncword2/2,3,1,0,3,2,0,1/
   data graymap/0,1,3,2/
   if(.not.allocated(work%one)) then
      allocate(work%one(0:65535,0:15),work%zero(0:65535,0:15),work%s2(0:65535))
      do i=0,65535
         do j=0,15
            work%one(i,j)=btest(i,j)
            work%zero(i,j)=.not.work%one(i,j)
         enddo
      enddo
   endif
   if(nss.ne.work%nss) then
      if(allocated(work%ci)) then
         if(size(work%ci,1)<nss) deallocate(work%ci)
      endif
      if(.not.allocated(work%ci)) allocate(work%ci(nss,0:3))
      twopi=8.0*atan(1.0)
      dphi=twopi/nss
      do itone=0,3
         dp=(itone-1.5)*dphi
         phi=0.0
         do j=1,nss
            work%ci(j,itone)=cmplx(cos(phi),sin(phi))
            phi=mod(phi+dp,twopi)
         enddo
      enddo
      work%nss=nss
   endif

   do k=1,NN
      i1=(k-1)*NSS
      do itone=0,3
         cs(itone,k)=sum(cd(i1:i1+nss-1)*conjg(work%ci(1:nss,itone)))
      enddo
      s4(0:3,k)=abs(cs(0:3,k))**2
   enddo

! Sync quality check
   is1=0
   is2=0
   is3=0
   is4=0
   is5=0
   badsync=.false.
   ibmax=0

   do k=1,8
      ip=maxloc(s4(:,k))
      if(isyncword1(k-1).eq.(ip(1)-1)) is1=is1+1
      ip=maxloc(s4(:,k+38))
      if(isyncword2(k-1).eq.(ip(1)-1)) is2=is2+1
      ip=maxloc(s4(:,k+76))
      if(isyncword1(k-1).eq.(ip(1)-1)) is3=is3+1
      ip=maxloc(s4(:,k+114))
      if(isyncword2(k-1).eq.(ip(1)-1)) is4=is4+1
      ip=maxloc(s4(:,k+152))
      if(isyncword1(k-1).eq.(ip(1)-1)) is5=is5+1
   enddo
   nsync=is1+is2+is3+is4+is5   !Number of correct hard sync symbols, 0-40
   badsync=.false.
   if(nsync .lt. 16) then
      badsync=.true.
      return
   endif

   call timer('seqcorrs',0)
   bitmetrics=0.0

! Process the frame in 8-symbol chunks. Use 1-symbol correlations to calculate
! 2-symbol correlations. Then use 2-symbol correlations to calculate 4-symbol
! correlations. Finally, use 4-symbol correlations to calculate 8-symbol corrs.
! This eliminates redundant calculations.

   do k=1,NN,8

      do m=1,8  ! do 4 1-symbol correlations for each of 8 symbs
         do n=1,4
            c1(n,m)=cs(graymap(n-1),k+m-1) 
            work%s2(n-1)=abs(c1(n,m))
         enddo
         ipt=(k-1)*2+2*(m-1)+1
         do ib=0,1
            bm=maxval(work%s2(0:3),work%one(0:3,1-ib)) - &
               maxval(work%s2(0:3),work%zero(0:3,1-ib))
            if(ipt+ib.gt.2*NN) cycle
            bitmetrics(ipt+ib,1)=bm
         enddo
      enddo

      do m=1,4  ! do 16 2-symbol correlations for each of 4 2-symbol groups
         do i=1,4
            do j=1,4
               is=(i-1)*4+j
               c2(is,m)=c1(i,2*m-1)-c1(j,2*m)
               work%s2(is-1)=abs(c2(is,m))
            enddo
         enddo
         ipt=(k-1)*2+4*(m-1)+1
         do ib=0,3
            bm=maxval(work%s2(0:15),work%one(0:15,3-ib)) - &
               maxval(work%s2(0:15),work%zero(0:15,3-ib))
            if(ipt+ib.gt.2*NN) cycle
            bitmetrics(ipt+ib,2)=bm
         enddo
      enddo
  
      do m=1,2 ! do 256 4-symbol corrs for each of 2 4-symbol groups
         do i=1,16
            do j=1,16
               is=(i-1)*16+j
               c4(is,m)=c2(i,2*m-1)+c2(j,2*m)
               work%s2(is-1)=abs(c4(is,m))
            enddo
         enddo 
         ipt=(k-1)*2+8*(m-1)+1
         do ib=0,7
            bm=maxval(work%s2(0:255),work%one(0:255,7-ib)) - &
               maxval(work%s2(0:255),work%zero(0:255,7-ib))
            if(ipt+ib.gt.2*NN) cycle
            bitmetrics(ipt+ib,3)=bm
         enddo
      enddo

      ! Combine the two four-symbol groups into eight-symbol correlations.
      do i=1,256
         do j=1,256
            is=(i-1)*256+j
            work%s2(is-1)=abs(c4(i,1)+c4(j,2))
         enddo
      enddo
      ipt=(k-1)*2+1
      do ib=0,15
         bm=maxval(work%s2(0:65535),work%one(0:65535,15-ib)) - &
            maxval(work%s2(0:65535),work%zero(0:65535,15-ib))
         if(ipt+ib.gt.2*NN) cycle
         bitmetrics(ipt+ib,4)=bm
      enddo

   enddo

   call timer('seqcorrs',1)

   hbits=0
   where(bitmetrics(:,1).ge.0) hbits=1
   ns1=count(hbits(  1: 16).eq.(/0,0,0,1,1,0,1,1,0,1,0,0,1,1,1,0/))
   ns2=count(hbits( 77: 92).eq.(/1,1,1,0,0,1,0,0,1,0,1,1,0,0,0,1/))
   ns3=count(hbits(153:168).eq.(/0,0,0,1,1,0,1,1,0,1,0,0,1,1,1,0/))
   ns4=count(hbits(229:244).eq.(/1,1,1,0,0,1,0,0,1,0,1,1,0,0,0,1/))
   ns5=count(hbits(305:320).eq.(/0,0,0,1,1,0,1,1,0,1,0,0,1,1,1,0/))
   nsync_qual=ns1+ns2+ns3+ns4+ns5

   if(nsync_qual.lt. 46) then
      badsync=.true.
      return
   endif

   call normalizebmet(bitmetrics(:,1),2*NN)
   call normalizebmet(bitmetrics(:,2),2*NN)
   call normalizebmet(bitmetrics(:,3),2*NN)
   call normalizebmet(bitmetrics(:,4),2*NN)

   scalefac=2.83
   bitmetrics=scalefac*bitmetrics

   return

end subroutine get_fst4_bitmetrics_owned

end module fst4_bitmetrics

subroutine get_fst4_bitmetrics(cd,nss,bitmetrics,s4,nsync_qual,badsync)
   use fst4_bitmetrics
   type(fst4_bitmetrics_workspace), save :: legacy
   complex cd(0:160*nss-1)
   real bitmetrics(320,4),s4(0:3,160)
   logical badsync
   call get_fst4_bitmetrics_owned(legacy,cd,nss,bitmetrics,s4,nsync_qual,badsync)
end subroutine
