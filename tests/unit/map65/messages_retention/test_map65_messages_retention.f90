program test_map65_messages_retention
  use iso_fortran_env, only: int64, real64
  use stdout_channel_mod
  use display_mod, only: display
  use message_history_mod, only: message_record, append_message, clear_messages, recent_messages, &
                                 message_expired
  implicit none
  integer :: missed, retained, begins_before

  ! A light band keeps exactly Timeout minutes of history.
  call run_band(40, 3, 60, missed, retained)
  call require(missed == 0, 'light band: every minute''s decodes are shown')
  call require(oldest_utc == 1219, '40-minute timeout keeps 40 minutes (1219-1259)')
  call run_band(10, 3, 60, missed, retained)
  call require(oldest_utc == 1249, '10-minute timeout keeps 10 minutes (1249-1259)')

  ! History retains the timeout interval while display shows the newest rows.
  call run_band(40, 25, 120, missed, retained)
  call require(missed == 0, 'busy band: newest decodes shown every minute')
  call require(retained == 1025, 'busy band: all 40 minutes remain in memory')
  call run_band(20, 18, 120, missed, retained)
  call require(missed == 0, '20-minute timeout, 18 decodes/min: newest shown every minute')
  call test_bandmap_capacity()
  call test_midnight_expiry()
  call test_bandmap_newest_across_groups()
  call test_expiry_edge()
  call test_bandmap_modes()
  call test_bandmap_compound_call()
  call clear_messages()
  shown = 0
  begins_before = display_begins
  call display(10, 0.010, 1200)
  call require(shown == 0, 'empty history emits no Messages rows')
  call require(display_begins == begins_before + 1, 'empty history emits a snapshot boundary')
  print '(a)', 'MAP65 Messages retention tests passed.'

contains
  subroutine run_band(nkeep, per_min, minutes, missed, retained)
    integer, intent(in) :: nkeep, per_min, minutes
    integer, intent(out) :: missed, retained
    integer :: k, minute, nutc
    character(len=22) :: msg
    character(len=83) :: rec
    real(real64) :: f0
    type(message_record), allocatable :: records(:)
    call clear_messages()
    missed = 0
    do minute = 0, minutes - 1
      nutc = 100*(12 + minute/60) + mod(minute, 60)
      do k = 1, per_min
        f0 = 144.0_real64 + 0.001_real64*(100 + 3*k)
        write(msg, '(a,i3.3,a)') 'CQ K', k, 'X FN42'
        write(rec, 1014) f0, 0, 0, 0, 0, 0.0, 0, 5, -20, nutc, msg, '#', ' ', '#B'
        call append_message(rec, nutc, f0, 0)
      enddo
      call display(nkeep, 0.010, nutc)
      shown = 0; shown_at_utc = 0; want_utc = nutc; oldest_utc = 9999
      call display(nkeep, 0.010, nutc)
      if (shown_at_utc /= per_min) missed = missed + 1
    enddo
    call recent_messages(2000, records)
    retained = size(records)
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  ! Records exactly Timeout minutes old by UTC are kept whatever mode stored
  ! them. A Q65 record is stored during the pass, up to about 20 s before a
  ! JT65 record stored at its end, so it is older by the wall clock.
  subroutine test_expiry_edge()
    call require(.not. message_expired(10, 600_int64, 10), 'edge: record stored at the end of the pass is kept')
    call require(.not. message_expired(10, 620_int64, 10), 'edge: record stored earlier in the pass is kept')
    call require(message_expired(11, 660_int64, 10), 'edge: record older than the Timeout by UTC expires')
    call require(message_expired(10, 86400_int64, 10), 'edge: same UTC minute a day later expires')
    call require(.not. message_expired(10, -1_int64, 10), 'edge: unknown storage time falls back to UTC age')
  end subroutine

  subroutine test_midnight_expiry()
    character(len=83) :: rec
    character(len=22) :: msg
    type(message_record), allocatable :: records(:)

    call clear_messages()
    msg = 'CQ K001X FN42'
    write(rec, 1014) 144.100_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 2358, msg, '#', ' ', '#B'
    call append_message(rec, 2358, 144.100_real64, 0)
    write(rec, 1014) 144.101_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 1, msg, '#', ' ', '#B'
    call append_message(rec, 1, 144.101_real64, 0)
    shown = 0; bandmap_entries = 0; first_bandmap_frequency = ' '
    call display(10, 0.010, 2)
    call recent_messages(10, records)
    call require(size(records) == 2, 'midnight: recent records remain')
    call require(first_shown_utc == 2358 .and. second_shown_utc == 1, &
                 'midnight: Messages preserve chronological order')
    call require(bandmap_entries == 1 .and. first_bandmap_frequency == '101', &
                 'midnight: Band Map uses the newest frequency for a call')
    call display(10, 0.010, 10)
    call recent_messages(10, records)
    call require(size(records) == 1 .and. records(1)%utc == 1, 'midnight: old record expires')
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  subroutine test_bandmap_newest_across_groups()
    character(len=22) :: msg
    character(len=83) :: rec

    call clear_messages()
    msg = 'CQ K001X FN42'
    write(rec, 1014) 144.200_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 1200, msg, '#', ' ', '#B'
    call append_message(rec, 1200, 144.200_real64, 0)
    write(rec, 1014) 144.100_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 1201, msg, '#', ' ', '#B'
    call append_message(rec, 1201, 144.100_real64, 0)
    bandmap_entries = 0; first_bandmap_frequency = ' '; first_bandmap_utc = ' '
    call display(10, 0.010, 1201)
    call require(bandmap_entries == 1 .and. first_bandmap_frequency == '100', &
                 'Band Map uses the newest decode across frequency groups')
    ! The waterfall label ages from this UTC, so it must be the newest decode's.
    call require(first_bandmap_utc == '1201', 'Band Map entry carries the newest decode''s UTC')
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  ! Band Map entries carry the decode's mode, for the waterfall label color.
  subroutine test_bandmap_modes()
    character(len=22) :: msg
    character(len=83) :: rec

    call clear_messages()
    msg = 'CQ K001X FN42'
    write(rec, 1014) 144.100_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 1200, msg, '#', ' ', '#B'
    call append_message(rec, 1200, 144.100_real64, 0)
    msg = 'CQ K002X FN42'
    write(rec, 1014) 144.200_real64, 0, 0, 0, 0, 0.0, 0, 5, -20, 1200, msg, ':', ' ', ':A'
    call append_message(rec, 1200, 144.200_real64, 0)
    bandmap_entries = 0; bandmap_modes = ' '
    call display(10, 0.010, 1200)
    call require(bandmap_entries == 2 .and. bandmap_modes(1:2) == '#:', &
                 'Band Map entries carry each decode''s mode')
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  ! A compound call (prefix/call) reaches the Band Map whole, with the mode
  ! and UTC after it, for the Band Map window and the waterfall label.
  subroutine test_bandmap_compound_call()
    character(len=22) :: msg
    character(len=83) :: rec

    call clear_messages()
    msg = 'CQ ER/EA8DBM'
    write(rec, 1014) 144.124_real64, -500, 0, 0, 0, 0.0, 0, 5, -20, 1200, msg, ':', ' ', ':A'
    call append_message(rec, 1200, 144.124_real64, -500)
    bandmap_entries = 0; bandmap_modes = ' '; first_bandmap_call = ' '; first_bandmap_utc = ' '
    call display(10, 0.010, 1200)
    call require(bandmap_entries == 1 .and. first_bandmap_call == 'ER/EA8DBM', &
                 'compound call reaches the Band Map whole')
    call require(bandmap_modes(1:1) == ':' .and. first_bandmap_utc == '1200', &
                 'compound call entry keeps its mode and UTC')
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  subroutine test_bandmap_capacity()
    integer :: k
    character(len=22) :: msg
    character(len=83) :: rec
    real(real64) :: f0

    call clear_messages()
    do k = 1, 600
      f0 = 144.0_real64 + 0.001_real64*(100 + k)
      write(msg, '(a,i3.3,a)') 'CQ K', k, 'X FN42'
      write(rec, 1014) f0, 0, 0, 0, 0, 0.0, 0, 5, -20, 1200, msg, '#', ' ', '#B'
      call append_message(rec, 1200, f0, 0)
    enddo
    shown = 0; shown_at_utc = 0; want_utc = 1200; bandmap_entries = 0
    first_bandmap_frequency = ' '
    call display(40, 0.010, 1200)
    call require(shown_at_utc == 600, '600 busy-band Messages rows are shown')
    call require(bandmap_entries == 600, 'Band Map includes every visible call')
    call require(first_bandmap_frequency == '101', 'Band Map remains frequency ordered')
1014 format(f8.3, i5, 3i3, f5.1, i4, i3, i4, i5.4, 4x, a22, 7x, 2a1, 2x, a2)
  end subroutine

  subroutine require(condition, description)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: description
    if (.not. condition) then
      print '(a)', 'FAIL: '//description
      error stop 1
    endif
  end subroutine
end program
