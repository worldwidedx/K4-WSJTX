subroutine get_q3list(fname,bDiskData,capacity,nlist,list)

  use types, only: q3list
  use, intrinsic :: iso_fortran_env, only: int64
  use q65_callers, only: Q65_MAX_CALLERS, Q65_ROW_WIDTH,             &
       q65_load_callers, q65_expire_callers
  use jpl_ephemeris_status, only: EPHEMERIS_INVALID_INPUT,           &
       EPHEMERIS_UNAVAILABLE
  implicit none

  character(len=*), intent(in) :: fname
  logical*1, intent(in) :: bDiskData
  integer, intent(in) :: capacity
  integer, intent(out) :: nlist
  character(len=*), intent(inout) :: list(*)
  character(len=8) :: grid6
  integer :: nt(8),now,nhist,status,i,j,mjd
  integer :: utc_year,utc_month,utc_day,ephemeris_result
  integer :: indx(Q65_MAX_CALLERS)
  real :: uth,xlon,xlat,RASun,DecSun,xLST,AzSun,ElSun,day
  real :: RAMoon,DecMoon,HA,AzMoon,ElMoon,vr,techo,age
  type(q3list) :: history(Q65_MAX_CALLERS),callers(Q65_MAX_CALLERS)

  nlist=0
  if(capacity.le.0 .or. len(list).lt.Q65_ROW_WIDTH) return
  call q65_load_callers(fname,history,nhist,status)
  if(status.ne.0 .or. nhist.eq.0) return
  now=time()
  call q65_expire_callers(history,nhist,now)
  call date_and_time(values=nt)
  call normalize_utc_calendar(nt,utc_year,utc_month,utc_day,uth)

  do i=1,nhist
     grid6=history(i)%grid//'mm'
     call grid2deg(grid6,xlon,xlat)
     call sun(utc_year,utc_month,utc_day,uth,-xlon,xlat,RASun,DecSun, &
          xLST,AzSun,ElSun,mjd,day)
     call moondopjpl(utc_year,utc_month,utc_day,uth,-xlon,xlat,RAMoon, &
          DecMoon,xLST,HA,AzMoon,ElMoon,vr,techo,ephemeris_result)
     if(ephemeris_result.eq.EPHEMERIS_INVALID_INPUT .or.             &
          ephemeris_result.eq.EPHEMERIS_UNAVAILABLE) cycle
     if(ElMoon.lt.-5.0 .and. (.not.bDiskData)) cycle
     nlist=nlist+1
     callers(nlist)=history(i)
     callers(nlist)%moonel=nint(ElMoon)
  enddo

  if(nlist.eq.0) return
  call indexx(callers(1:nlist)%nfreq,nlist,indx)
  nlist=min(nlist,capacity)
  do i=1,nlist
     j=indx(i)
     age=real(int(now,int64)-int(callers(j)%nsec,int64))/3600.0
     write(list(i),1000) i,callers(j)%nfreq,callers(j)%call,           &
          callers(j)%grid,callers(j)%moonel,age,char(0)
1000 format(i2,'.',i6,2x,a6,2x,a4,i5,f7.1,1x,a1)
  enddo

end subroutine get_q3list

subroutine normalize_utc_calendar(values,year,month,day,uth)

  implicit none

  integer, intent(in) :: values(8)
  integer, intent(out) :: year,month,day
  real, intent(out) :: uth
  real :: utc_seconds

  year=values(1)
  month=values(2)
  day=values(3)
  utc_seconds=3600.0*values(5) + 60.0*values(6) + values(7) +         &
       0.001*values(8) - 60.0*values(4)

  if(utc_seconds.lt.0.0) then
     utc_seconds=utc_seconds + 86400.0
     day=day - 1
     if(day.lt.1) then
        month=month - 1
        if(month.lt.1) then
           year=year - 1
           month=12
        endif
        day=days_in_month(year,month)
     endif
  else if(utc_seconds.ge.86400.0) then
     utc_seconds=utc_seconds - 86400.0
     day=day + 1
     if(day.gt.days_in_month(year,month)) then
        day=1
        month=month + 1
        if(month.gt.12) then
           year=year + 1
           month=1
        endif
     endif
  endif
  uth=utc_seconds/3600.0

contains

  integer function days_in_month(calendar_year,calendar_month)
    integer, intent(in) :: calendar_year,calendar_month
    logical :: leap_year

    select case(calendar_month)
    case(4,6,9,11)
       days_in_month=30
    case(2)
       leap_year=mod(calendar_year,400).eq.0 .or.                    &
            (mod(calendar_year,4).eq.0 .and. mod(calendar_year,100).ne.0)
       days_in_month=28
       if(leap_year) days_in_month=29
    case default
       days_in_month=31
    end select
  end function days_in_month

end subroutine normalize_utc_calendar

subroutine rm_q3list(fname,dxcall)

  use types, only: q3list
  use q65_callers, only: Q65_MAX_CALLERS, q65_load_callers,          &
       q65_save_callers, q65_remove_caller
  implicit none

  character(len=*), intent(in) :: fname,dxcall
  type(q3list) :: callers(Q65_MAX_CALLERS)
  integer :: count,status

  call q65_load_callers(fname,callers,count,status)
  if(status.ne.0) return
  call q65_remove_caller(callers,count,dxcall)
  call q65_save_callers(fname,callers,count,status)

end subroutine rm_q3list

subroutine jpl_setup(fname)
  use jpl_ephemeris_status, only: reset_jpl_reader
  character*256 fname,jpleph_file_name
  common/jplcom/jpleph_file_name
  j = index(fname,char(0))
  if(j.eq.0) then
     jpleph_file_name=fname
  else
     jpleph_file_name(1:j)=fname(1:j)
     jpleph_file_name(j:)=' '
  endif
  call reset_jpl_reader()
  return
end subroutine jpl_setup
