program test_q65_callers
  use types, only: q3list
  use q65_callers
  use, intrinsic :: iso_fortran_env, only: int16
  implicit none
  integer, parameter :: now=1000000
  character(len=*), parameter :: filename='test_q65_callers.dat'
  type(q3list) :: callers(Q65_MAX_CALLERS),loaded(Q65_MAX_CALLERS)
  integer :: count,status,n,unit,i
  character(len=6) :: callsign

  call remove_file()
  call q65_load_callers(filename,loaded,n,status)
  call expect(status.eq.0 .and. n.eq.0,'missing history is empty')
  call expect(all(loaded%call.eq.'') .and. all(loaded%nsec.eq.0),'empty history is initialized')
  call check_expiry()
  call check_capacity()
  call check_persistence()
  call remove_file()
  print *, 'Q65 caller history tests passed'

contains

  subroutine expect(condition,message)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: message
    if(condition) return
    print *, message
    error stop 1
  end subroutine expect

  subroutine check_expiry()
    callers=q3list('','',0,0,0)
    callers(1)=q3list('K1AAA','FN42',now-86401,1000,0)
    callers(2)=q3list('K1BBB','FN42',now-86401,1100,0)
    callers(3)=q3list('K1CCC','FN42',now,1200,0)
    count=3
    call q65_expire_callers(callers,count,now)
    call expect(count.eq.1 .and. callers(1)%call.eq.'K1CCC','adjacent expired entries are removed')
    call q65_expire_callers(callers,count,now)
    call expect(count.eq.1 .and. callers(1)%call.eq.'K1CCC','expiry is idempotent')
    callers(1)=q3list('K1AAA','FN42',now-86401,1000,0)
    callers(2)=q3list('K1BBB','FN42',now,1100,0)
    callers(3)=q3list('K1CCC','FN42',now-86401,1200,0)
    count=3
    call q65_expire_callers(callers,count,now)
    call expect(count.eq.1 .and. callers(1)%call.eq.'K1BBB','fresh caller survives expired neighbors')
    callers(1)=q3list('K1AAA','FN42',now-86400,1000,0)
    callers(2)=q3list('K1BBB','FN42',now+1,1100,0)
    count=2
    call q65_expire_callers(callers,count,now)
    call expect(count.eq.2,'24-hour boundary and future timestamps are retained')
    call q65_expire_callers(callers,count,now+86402)
    call expect(count.eq.0 .and. all(callers%call.eq.''),'all-expired history is empty')
  end subroutine check_expiry

  subroutine check_capacity()
    callers=q3list('','',0,0,0)
    count=0
    do i=1,Q65_MAX_CALLERS
       write(callsign,'(a2,i4.4)') 'K1',i
       call q65_record_caller(callers,count,1000+i,'K1ABC '//callsign//' FN42',now+i)
    enddo
    call expect(count.eq.Q65_MAX_CALLERS,'history reaches full capacity')
    call q65_record_caller(callers,count,2000,'K1ABC K10001 R EM10',now+100)
    call expect(count.eq.Q65_MAX_CALLERS .and. callers(1)%nsec.eq.now+100,'refresh does not add a caller')
    call expect(callers(1)%grid.eq.'FN42' .and. callers(1)%nfreq.eq.2000,'refresh preserves grid and updates frequency')
    call q65_record_caller(callers,count,2100,'K1ABC K19999 FN42',now+101)
    call expect(count.eq.Q65_MAX_CALLERS,'overflow retains capacity')
    call expect(callers(1)%call.eq.'K10001' .and. callers(2)%call.eq.'K10003', &
         'least recently heard caller is evicted after refresh')
    call expect(callers(count)%call.eq.'K19999','new caller enters full history')
    call q65_save_callers(filename,callers,count,status)
    call expect(status.eq.0,'full history saves')
    call q65_load_callers(filename,loaded,n,status)
    call expect(status.eq.0 .and. n.eq.count,'full history reloads')
    call expect(all(loaded%call.eq.callers%call) .and. all(loaded%nsec.eq.callers%nsec), &
         'eviction and refresh survive reload')

    callers%nsec=now
    call q65_record_caller(callers,count,2200,'K1ABC K18888 FN42',now+1)
    call expect(callers(1)%call.eq.'K10003','equal timestamps evict first stored caller')
    call q65_record_caller(callers,count,2300,'K1ABC K17777/P FN42',now+2)
    call expect(.not.any(callers%call.eq.'K17777'),'compound calls are ignored')
    call q65_record_caller(callers,count,2300,'K1ABC K17777 RR73',now+2)
    call expect(.not.any(callers%call.eq.'K17777'),'RR73 does not introduce a caller')
    call q65_remove_caller(callers,count,'K18888')
    call expect(count.eq.Q65_MAX_CALLERS-1,'final caller can be removed at capacity')
    call q65_remove_caller(callers,count,'K18888')
    call expect(count.eq.Q65_MAX_CALLERS-1,'absent caller removal is harmless')
    do while(count.gt.0)
       callsign=callers(1)%call
       call q65_remove_caller(callers,count,callsign)
    enddo
    call q65_save_callers(filename,callers,count,status)
    call expect(status.eq.0,'empty history saves')
    call q65_load_callers(filename,loaded,n,status)
    call expect(status.eq.0 .and. n.eq.0,'empty history reloads')

    callers(1)=q3list('K1OLD','FN42',now-86401,1000,0)
    count=1
    call q65_record_caller(callers,count,1500,'K1ABC K1NEW FN42',now)
    call expect(count.eq.1 .and. callers(1)%call.eq.'K1NEW','recording expires old callers first')
  end subroutine check_capacity

  subroutine check_persistence()
    integer :: legacy_count,saved_count

    callers=q3list('K1ABC','FN42',now,1500,0)
    do legacy_count=40,50,10
       open(newunit=unit,file=filename,status='replace',form='unformatted')
       write(unit) legacy_count
       write(unit) callers(1:legacy_count)
       close(unit)
       call q65_load_callers(filename,loaded,n,status)
       call expect(status.eq.0 .and. n.eq.legacy_count,'legacy binary history loads')
       call expect(all(loaded(1:n)%call.eq.'K1ABC'),'legacy caller records are preserved')
    enddo

    do saved_count=-1,Q65_MAX_CALLERS+1,Q65_MAX_CALLERS+2
       open(newunit=unit,file=filename,status='replace',form='unformatted')
       write(unit) saved_count
       close(unit)
       call q65_load_callers(filename,loaded,n,status)
       call expect(status.ne.0 .and. n.eq.0 .and. all(loaded%call.eq.''),'invalid count is rejected safely')
       open(newunit=unit,file=filename,status='old',form='unformatted')
       read(unit) legacy_count
       close(unit)
       call expect(legacy_count.eq.saved_count,'failed load leaves malformed file unchanged')
    enddo

    open(newunit=unit,file=filename,status='replace',form='unformatted')
    write(unit) 2
    write(unit) callers(1:1)
    close(unit)
    call q65_load_callers(filename,loaded,n,status)
    call expect(status.ne.0 .and. n.eq.0 .and. all(loaded%nsec.eq.0),'short caller record is rejected safely')
    call q65_save_callers(filename,callers,Q65_MAX_CALLERS+1,status)
    call expect(status.ne.0,'over-capacity save is rejected')
    open(newunit=unit,file=filename,status='old',form='unformatted')
    read(unit) saved_count
    close(unit)
    call expect(saved_count.eq.2,'rejected save does not replace existing file')

    open(newunit=unit,file=filename,status='replace',form='unformatted')
    close(unit)
    call q65_load_callers(filename,loaded,n,status)
    call expect(status.eq.0 .and. n.eq.0,'legacy empty file is a normal empty history')
    open(newunit=unit,file=filename,status='replace',form='unformatted',access='stream')
    write(unit) 1_int16
    close(unit)
    call q65_load_callers(filename,loaded,n,status)
    call expect(status.ne.0 .and. n.eq.0,'truncated count is rejected safely')
  end subroutine check_persistence

  subroutine remove_file()
    logical :: exists
    inquire(file=filename,exist=exists)
    if(.not.exists) return
    open(newunit=unit,file=filename,status='old')
    close(unit,status='delete')
  end subroutine remove_file
end program test_q65_callers
