program test_fst4_osd_depth

  use fst4_bitmetrics, only: fst4_bitmetrics_workspace,get_fst4_bitmetrics_owned
  use fst4_osd_workspace, only: fst4_osd_workspace_type
  use fastosd240_74_module, only: fastosd240_74_owned
  use ieee_arithmetic, only: ieee_is_finite
  implicit none

  integer, parameter :: n = 240
  real :: llr(n)
  integer(kind=1) :: apmask(n)
  integer(kind=1) :: message_zero(74), message_negative(74)
  integer(kind=1) :: message101_zero(101), message101_negative(101)
  integer(kind=1) :: cw_zero(n), cw_negative(n)
  real :: llr128(128)
  integer(kind=1) :: apmask128(128)
  integer(kind=1) :: message77_zero(77), message77_negative(77)
  integer(kind=1) :: cw128_zero(128), cw128_negative(128)
  integer :: nhard_zero, nhard_negative
  real :: dmin_zero, dmin_negative
  integer :: i
  integer :: nhard128_zero, nhard128_negative
  real :: dmin128_zero, dmin128_negative
  external :: osd240_74, osd240_101, osd128_90

  do i = 1, n
     llr(i) = real(mod(7*i, 23) - 11)
  end do
  apmask = 0

  call osd240_74(llr, 50, apmask, 0, message_zero, cw_zero, nhard_zero, dmin_zero)
  call osd240_74(llr, 50, apmask, -1, message_negative, cw_negative, &
       nhard_negative, dmin_negative)
  call require(all(message_zero == message_negative), &
       'negative (240,74) OSD depth matches zero depth')
  call require(all(cw_zero == cw_negative), &
       'negative (240,74) OSD depth preserves the codeword')
  call require(nhard_zero == nhard_negative .and. dmin_zero == dmin_negative, &
       'negative (240,74) OSD depth preserves metrics')

  call osd240_101(llr, 77, apmask, 0, message101_zero, cw_zero, &
       nhard_zero, dmin_zero)
  call osd240_101(llr, 77, apmask, -1, message101_negative, cw_negative, &
       nhard_negative, dmin_negative)
  call require(all(message101_zero == message101_negative), &
       'negative (240,101) OSD depth matches zero depth')
  call require(all(cw_zero == cw_negative), &
       'negative (240,101) OSD depth preserves the codeword')
  call require(nhard_zero == nhard_negative .and. dmin_zero == dmin_negative, &
       'negative (240,101) OSD depth preserves metrics')

  do i = 1, 128
     llr128(i) = real(mod(11*i, 29) - 14)
  end do
  apmask128 = 0
  call osd128_90(llr128, apmask128, 0, message77_zero, cw128_zero, &
       nhard128_zero, dmin128_zero)
  call osd128_90(llr128, apmask128, -1, message77_negative, cw128_negative, &
       nhard128_negative, dmin128_negative)
  call require(all(message77_zero == message77_negative), &
       'negative (128,90) OSD depth matches zero depth')
  call require(all(cw128_zero == cw128_negative), &
       'negative (128,90) OSD depth preserves the codeword')
  call require(nhard128_zero == nhard128_negative .and. &
       dmin128_zero == dmin128_negative, &
       'negative (128,90) OSD depth preserves metrics')

  call check_osd_cache()
  call check_bitmetric_cache()

  print '(a)', 'FST4 OSD depth and cache tests passed'

contains

  subroutine check_osd_cache()
    type(fst4_osd_workspace_type), target :: reused,fresh
    integer, parameter :: effective_bits(3)=[66,50,66]
    integer :: pass,bits
    do pass=1,size(effective_bits)
       bits=effective_bits(pass)
       call fastosd240_74_owned(reused,llr,bits,apmask,3,message_zero,cw_zero,nhard_zero,dmin_zero)
       call fastosd240_74_owned(fresh,llr,bits,apmask,3,message_negative,cw_negative,nhard_negative,dmin_negative)
       call require(all(message_zero==message_negative).and.all(cw_zero==cw_negative), &
            'reused FST4W OSD workspace matches a fresh workspace')
       call require(nhard_zero==nhard_negative.and.dmin_zero==dmin_negative, &
            'reused FST4W OSD workspace preserves decode metrics')
       call fresh%destroy()
    enddo
    call require(allocated(reused%generator(66)%matrix).and.allocated(reused%generator(50)%matrix), &
         'FST4W retains both effective-length generator caches')
    call reused%destroy()
    call require(.not.allocated(reused%matrix).and..not.allocated(reused%generator(66)%matrix), &
         'OSD destruction releases reusable storage')
  end subroutine check_osd_cache

  subroutine check_bitmetric_cache()
    type(fst4_bitmetrics_workspace), target :: reused,fresh
    complex, allocatable :: frame(:),saved_wave(:,:)
    real :: metrics(320,4),reference(320,4),power(0:3,160),reference_power(0:3,160)
    integer :: tones(160),nss,pass,symbol,sample,quality,reference_quality
    integer, parameter :: periods(3)=[12,24,12]
    integer, parameter :: sync1(8)=[0,1,3,2,1,0,2,3],sync2(8)=[2,3,1,0,3,2,0,1]
    real :: phase,twopi
    logical :: bad,reference_bad
    twopi=8*atan(1.0)
    do symbol=1,160
       tones(symbol)=mod(3*symbol+symbol/7,4)
    enddo
    tones(1:8)=sync1
    tones(39:46)=sync2
    tones(77:84)=sync1
    tones(115:122)=sync2
    tones(153:160)=sync1
    do pass=1,size(periods)
       nss=periods(pass)
       allocate(frame(0:160*nss-1))
       do symbol=1,160
          do sample=0,nss-1
             phase=twopi*(tones(symbol)-1.5)*sample/nss
             frame((symbol-1)*nss+sample)=cmplx(cos(phase),sin(phase)) + &
                  0.03*cmplx(cos(real(17*symbol+3*sample)),sin(real(11*symbol+7*sample)))
          enddo
       enddo
       call get_fst4_bitmetrics_owned(reused,frame,nss,metrics,power,quality,bad)
       call require(.not.bad.and.quality==80,'ideal frame reaches complete sync-bit processing')
       call require(reused%nss==nss.and.size(reused%ci,1)>=nss,'cache key follows symbol-size changes')
       if(pass==3) call require(size(reused%ci,1)==24,'smaller symbols retain workspace capacity')
       call require(all(ieee_is_finite(metrics)),'finite ideal-frame bit metrics')
       if(allocated(fresh%ci)) deallocate(fresh%ci,fresh%one,fresh%zero,fresh%s2)
       fresh%nss=0
       call get_fst4_bitmetrics_owned(fresh,frame,nss,reference,reference_power,reference_quality,reference_bad)
       call require(.not.reference_bad.and.reference_quality==quality,'fresh and reused cache sync quality')
       call require(all(metrics==reference).and.all(power==reference_power),'cache invalidation matches fresh workspace')
       saved_wave=reused%ci(1:nss,:)
       call get_fst4_bitmetrics_owned(reused,frame,nss,metrics,power,quality,bad)
       call require(all(reused%ci(1:nss,:)==saved_wave),'same symbol size retains ideal waveforms')
       call require(all(metrics==reference),'same symbol size retains bit metrics')
       deallocate(frame)
    enddo
  end subroutine check_bitmetric_cache

  subroutine require(condition, description)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: description

    if (.not. condition) then
       print '(a)', 'FAIL: '//description
       error stop 1
    end if
  end subroutine require

end program test_fst4_osd_depth
