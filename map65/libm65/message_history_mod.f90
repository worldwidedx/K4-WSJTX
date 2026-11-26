module message_history_mod
  use iso_fortran_env, only: int64, real64
  implicit none
  private

  type, public :: message_record
    character(len=83) :: line
    integer :: utc
    real :: frequency_khz
    integer(int64) :: inserted_tick
  end type message_record

  type(message_record), allocatable :: history(:)
  integer :: history_count = 0

  public :: append_message, clear_messages, expire_messages, recent_messages, has_message, message_expired

contains

  subroutine append_message(line, nutc, frequency_mhz, ndf)
    character(len=*), intent(in) :: line
    integer, intent(in) :: nutc, ndf
    real(real64), intent(in) :: frequency_mhz
    type(message_record), allocatable :: larger(:)
    integer :: capacity

    if (.not. allocated(history)) allocate(history(128))
    if (history_count == size(history)) then
      capacity = 2*size(history)
      allocate(larger(capacity))
      larger(:history_count) = history
      call move_alloc(larger, history)
    endif

    history_count = history_count + 1
    history(history_count)%line = line
    history(history_count)%utc = 60*(nutc/100) + mod(nutc, 100)
    history(history_count)%frequency_khz = &
      real(1000.0_real64*(frequency_mhz - 144.0_real64) + 0.001_real64*ndf)
    call system_clock(count=history(history_count)%inserted_tick)
  end subroutine append_message

  subroutine clear_messages()
    history_count = 0
  end subroutine clear_messages

  subroutine expire_messages(nutc, nkeep)
    integer, intent(in) :: nutc, nkeep
    integer :: i, kept, now
    integer(int64) :: now_tick, tick_rate, stored_s

    now = 60*(nutc/100) + mod(nutc, 100)
    call system_clock(count=now_tick, count_rate=tick_rate)
    kept = 0
    do i = 1, history_count
      stored_s = -1
      if (tick_rate > 0 .and. now_tick >= history(i)%inserted_tick) &
        stored_s = (now_tick - history(i)%inserted_tick)/tick_rate
      if (message_expired(modulo(now - history(i)%utc, 1440), stored_s, nkeep)) cycle
      kept = kept + 1
      if (kept /= i) history(kept) = history(i)
    enddo
    history_count = kept
  end subroutine expire_messages

  ! utc_age_min decides expiry. stored_s (seconds since the record was
  ! stored, -1 if unknown) only guards against the same UTC minute on a
  ! later day, so it allows a margin: a record can be stored up to a pass
  ! length before the display that ages it, so at the nkeep edge its
  ! wall-clock age exceeds 60*nkeep.
  logical function message_expired(utc_age_min, stored_s, nkeep)
    integer, intent(in) :: utc_age_min, nkeep
    integer(int64), intent(in) :: stored_s
    message_expired = utc_age_min > nkeep .or. stored_s > 60_int64*(nkeep + 2)
  end function message_expired

  subroutine recent_messages(max_records, records)
    integer, intent(in) :: max_records
    type(message_record), allocatable, intent(out) :: records(:)
    integer :: count

    count = min(history_count, max_records)
    allocate(records(count))
    if (count > 0) records = history(history_count-count+1:history_count)
  end subroutine recent_messages

  logical function has_message(frequency_mhz, nutc, decoded)
    real(real64), intent(in) :: frequency_mhz
    integer, intent(in) :: nutc
    character(len=*), intent(in) :: decoded
    character(len=83) :: probe
    integer :: i

    probe = ' '
    write(probe(1:8), '(f8.3)') frequency_mhz
    write(probe(39:43), '(i5.4)') nutc
    probe(48:69) = decoded

    has_message = .false.
    do i = history_count, 1, -1
      if (history(i)%line(1:8) /= probe(1:8)) cycle
      if (history(i)%line(39:43) /= probe(39:43)) cycle
      if (history(i)%line(48:69) /= probe(48:69)) cycle
      has_message = .true.
      return
    enddo
  end function has_message

end module message_history_mod
