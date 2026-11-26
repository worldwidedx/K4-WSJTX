program test_q65_caller_display
  use types, only: q3list
  use q65_callers
  use, intrinsic :: iso_fortran_env, only: int8
  implicit none

  type(q3list) :: callers(Q65_MAX_CALLERS),loaded(Q65_MAX_CALLERS)
  character(len=Q65_ROW_WIDTH) :: rows(Q65_MAX_CALLERS)
  character(len=256) :: ephemeris
  character(len=*), parameter :: path='caller-display-history.dat'
  character(len=*), parameter :: other='caller-display-removal.dat'
  character(len=*), parameter :: malformed='caller-display-malformed.dat'
  integer(int8), allocatable :: before(:),after(:)
  integer :: i,now,count,status,unit
  logical*1 :: disk_data

  ephemeris='missing-caller-display-ephemeris'
  call jpl_setup(ephemeris)
  now=time()
  do i=1,Q65_MAX_CALLERS
     write(callers(i)%call,'(a1,i5.5)') 'K',i
     callers(i)%grid='FN42'
     callers(i)%nsec=now
     callers(i)%nfreq=1500-i
     callers(i)%moonel=99
  enddo
  callers(1)%nsec=now-90000
  call q65_save_callers(path,callers,Q65_MAX_CALLERS,status)
  call require(status.eq.0,'save mixed-age history')
  call read_bytes(path,before)

  disk_data=.true.
  call get_q3list(path,disk_data,Q65_MAX_CALLERS,count,rows)
  call require(count.eq.Q65_MAX_CALLERS-1,'display excludes expired callers')
  call require(rows(1)(12:17).eq.callers(Q65_MAX_CALLERS)%call, &
       'display sorts callers by frequency')
  call require(rows(count)(12:17).eq.callers(2)%call,'display retains every fresh caller')
  call require(iachar(rows(count)(Q65_ROW_WIDTH:Q65_ROW_WIDTH)).eq.0,'row terminator')
  call require_unchanged(path,before,'disk display preserves history bytes')

  disk_data=.false.
  call get_q3list(path,disk_data,Q65_MAX_CALLERS,count,rows)
  call require_unchanged(path,before,'live visibility filtering preserves history bytes')
  disk_data=.true.
  rows='untouched'
  call get_q3list(path,disk_data,2,count,rows)
  call require(count.eq.2,'display respects output capacity')
  call require(all(rows(3:).eq.'untouched'),'display does not write beyond capacity')
  call require(rows(1)(12:17).eq.callers(Q65_MAX_CALLERS)%call, &
       'bounded display preserves frequency ordering')
  rows='untouched'
  call get_q3list(path,disk_data,0,count,rows)
  call require(count.eq.0 .and. all(rows.eq.'untouched'),'zero capacity leaves output untouched')

  callers(1)%nsec=now
  call q65_save_callers(path,callers,Q65_MAX_CALLERS,status)
  call require(status.eq.0,'save full history')
  call read_bytes(path,before)
  call get_q3list(path,disk_data,Q65_MAX_CALLERS,count,rows)
  call require(count.eq.Q65_MAX_CALLERS,'display includes full caller capacity')
  call require_unchanged(path,before,'full display preserves history bytes')

  loaded=q3list('','',0,0,0)
  loaded(1)=q3list('K1ABC','FN42',now,1500,0)
  call q65_save_callers(other,loaded,1,status)
  call require(status.eq.0,'save separate removal history')
  call rm_q3list(other,'K1ABC')
  call q65_load_callers(other,loaded,count,status)
  call require(status.eq.0 .and. count.eq.0,'remove final caller using explicit path')
  call require_unchanged(path,before,'removal leaves previously displayed history untouched')

  open(newunit=unit,file=malformed,form='unformatted',status='replace')
  write(unit) Q65_MAX_CALLERS+1
  close(unit)
  call read_bytes(malformed,before)
  call get_q3list(malformed,disk_data,Q65_MAX_CALLERS,count,rows)
  call require(count.eq.0,'malformed history displays empty')
  call rm_q3list(malformed,'K1ABC')
  call require_unchanged(malformed,before,'failed load never overwrites malformed history')

  call delete_file(path)
  call delete_file(other)
  call delete_file(malformed)
  deallocate(before,after)
  print *, 'PASS Q65 caller display and removal'

contains

  subroutine require(condition,message)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: message
    if(.not.condition) then
       print *, 'FAIL: '//message
       stop 1
    endif
  end subroutine require

  subroutine read_bytes(filename,bytes)
    character(len=*), intent(in) :: filename
    integer(int8), allocatable, intent(out) :: bytes(:)
    integer :: n,u
    inquire(file=filename,size=n)
    allocate(bytes(n))
    open(newunit=u,file=filename,status='old',access='stream',form='unformatted',action='read')
    read(u) bytes
    close(u)
  end subroutine read_bytes

  subroutine require_unchanged(filename,expected,message)
    character(len=*), intent(in) :: filename,message
    integer(int8), intent(in) :: expected(:)
    call read_bytes(filename,after)
    call require(size(after).eq.size(expected),message)
    call require(all(after.eq.expected),message)
  end subroutine require_unchanged

  subroutine delete_file(filename)
    character(len=*), intent(in) :: filename
    integer :: u
    open(newunit=u,file=filename,status='old')
    close(u,status='delete')
  end subroutine delete_file
end program test_q65_caller_display
