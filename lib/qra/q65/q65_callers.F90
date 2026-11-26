#include "q65_limits.h"
module q65_callers
  use types, only: q3list
  use, intrinsic :: iso_fortran_env, only: int64
  implicit none
  private
  integer, parameter, public :: Q65_MAX_CALLERS=Q65_CALLER_CAPACITY
  integer, parameter, public :: Q65_ROW_WIDTH=Q65_CALLER_ROW_WIDTH
  integer, parameter, public :: Q65_MAX_CODEWORDS=Q65_AP_LIST_CAPACITY
  public :: q65_load_callers,q65_save_callers,q65_expire_callers
  public :: q65_record_caller,q65_remove_caller

contains

  subroutine q65_load_callers(filename,callers,count,status)
    character(len=*), intent(in) :: filename
    type(q3list), intent(out) :: callers(Q65_MAX_CALLERS)
    integer, intent(out) :: count,status
    type(q3list) :: loaded(Q65_MAX_CALLERS)
    integer :: unit,n,close_status,file_size
    logical :: exists

    callers=q3list('','',0,0,0)
    count=0
    exists=.false.
    file_size=-1
    inquire(file=filename,exist=exists,size=file_size,iostat=status)
    if(status.ne.0) return
    if(.not.exists .or. file_size.eq.0) return
    open(newunit=unit,file=filename,status='old',form='unformatted',action='read',iostat=status)
    if(status.ne.0) return
    loaded=q3list('','',0,0,0)
    read(unit,iostat=status) n
    if(status.eq.0) then
       if(n.lt.0 .or. n.gt.Q65_MAX_CALLERS) then
          status=1
       else
          read(unit,iostat=status) loaded(1:n)
       endif
    endif
    close(unit,iostat=close_status)
    if(status.eq.0) status=close_status
    if(status.ne.0) return
    callers=loaded
    count=n
  end subroutine q65_load_callers

  subroutine q65_save_callers(filename,callers,count,status)
    character(len=*), intent(in) :: filename
    type(q3list), intent(in) :: callers(Q65_MAX_CALLERS)
    integer, intent(in) :: count
    integer, intent(out) :: status
    integer :: unit,close_status

    status=1
    if(count.lt.0 .or. count.gt.Q65_MAX_CALLERS) return
    open(newunit=unit,file=filename,status='replace',form='unformatted',action='write',iostat=status)
    if(status.ne.0) return
    write(unit,iostat=status) count
    if(status.eq.0) write(unit,iostat=status) callers(1:count)
    close(unit,iostat=close_status)
    if(status.eq.0) status=close_status
  end subroutine q65_save_callers

  subroutine q65_expire_callers(callers,count,now)
    type(q3list), intent(inout) :: callers(Q65_MAX_CALLERS)
    integer, intent(inout) :: count
    integer, intent(in) :: now
    integer :: i,kept

    kept=0
    do i=1,count
       if(int(now,int64)-int(callers(i)%nsec,int64).gt.86400_int64) cycle
       kept=kept+1
       callers(kept)=callers(i)
    enddo
    count=kept
    callers(count+1:)=q3list('','',0,0,0)
  end subroutine q65_expire_callers

  subroutine q65_record_caller(callers,count,freq,msg,now)
    type(q3list), intent(inout) :: callers(Q65_MAX_CALLERS)
    integer, intent(inout) :: count
    integer, intent(in) :: freq,now
    character(len=*), intent(in) :: msg
    character(len=37) :: text
    character(len=6) :: call
    character(len=4) :: grid
    integer :: i,i0,i1,i2,oldest

    call q65_expire_callers(callers,count,now)
    text=msg
    if(index(text,'/').gt.0) return
    i0=index(text,' R ')
    if(i0.ge.7) text=text(1:i0)//text(i0+3:)
    i1=index(text,' ')
    if(i1.lt.4 .or. i1.gt.13) return
    i2=index(text(i1+1:),' ')+i1
    if(i2.le.i1+1 .or. i2+4.gt.len(text)) return
    call=text(i1+1:i2-1)
    grid=text(i2+1:i2+4)
    do i=1,count
       if(callers(i)%call.ne.call) cycle
       callers(i)%nsec=now
       callers(i)%nfreq=freq
       return
    enddo
    if(.not.valid_grid(grid)) return
    if(count.eq.Q65_MAX_CALLERS) then
       oldest=minloc(callers(1:count)%nsec,dim=1)
       callers(oldest:count-1)=callers(oldest+1:count)
       count=count-1
    endif
    count=count+1
    callers(count)=q3list(call,grid,now,freq,0)
  end subroutine q65_record_caller

  subroutine q65_remove_caller(callers,count,call)
    type(q3list), intent(inout) :: callers(Q65_MAX_CALLERS)
    integer, intent(inout) :: count
    character(len=*), intent(in) :: call
    character(len=6) :: target
    integer :: i

    target=call
    do i=1,count
       if(callers(i)%call.ne.target) cycle
       callers(i:count-1)=callers(i+1:count)
       callers(count)=q3list('','',0,0,0)
       count=count-1
       return
    enddo
  end subroutine q65_remove_caller

  logical function valid_grid(grid)
    character(len=4), intent(in) :: grid
    valid_grid=grid(1:1).ge.'A' .and. grid(1:1).le.'R' .and. &
         grid(2:2).ge.'A' .and. grid(2:2).le.'R' .and. &
         grid(3:3).ge.'0' .and. grid(3:3).le.'9' .and. &
         grid(4:4).ge.'0' .and. grid(4:4).le.'9' .and. grid.ne.'RR73'
  end function valid_grid
end module q65_callers
