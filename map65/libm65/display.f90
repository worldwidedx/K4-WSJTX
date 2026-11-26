module display_mod
  implicit none
contains

subroutine display(nkeep, ftol, nutc)
  use stdout_channel_mod, only: write_stdout
  use message_history_mod, only: message_record, expire_messages, recent_messages
  use indexx_mod, only: indexx
  implicit none

  ! Arguments
  integer, intent(in) :: nkeep
  real,    intent(in) :: ftol
  integer, intent(in) :: nutc

  ! A busy band may retain more history than the Messages window can display.
  integer, parameter :: MAXLINES = 800

  ! Keep large buffers off the decoder thread's stack.
  integer, save   :: indx(MAXLINES), group_order(MAXLINES), call_order(MAXLINES)
  character(len=83), save :: line(MAXLINES)
  character(len=63)  :: out, out0
  ! 12 characters hold the longest compound calls (e.g. ER/EA8DBM, up to 11)
  ! plus a trailing space.
  character(len=12)  :: callsign, seen_calls(MAXLINES)
  ! Then the record's mode (# JT65, : Q65) and UTC (hhmm), so the GUI can
  ! color the waterfall callsign label and age it from the call's latest
  ! decode rather than from each redisplay.
  character(len=28)  :: freqcall(MAXLINES)

  real, save      :: freqkHz(MAXLINES), call_freqkHz(MAXLINES)
  type(message_record), allocatable :: records(:)
  integer         :: now

  character(len=83)  :: livecq2, livecq3
  character(len=128) :: linenew
  character(len=64)  :: linenew2

  integer :: i, j, group_first, group_last, group_size
  integer :: nz, nage, iage, nquad
  integer :: i1, i2, len, nc

  ! Initialize some scalars/arrays defensively
  out0      = ' '
  freqcall  = '            '

  call expire_messages(nutc, nkeep)
  call recent_messages(MAXLINES, records)
  call write_stdout('<Map65DisplayBegin>'//new_line('a'))
  nz = size(records)
  if (nz < 1) then
     return
  endif

  now = 60*(nutc/100) + mod(nutc, 100)
  nquad = max(nkeep/4, 3)
  do i = 1, nz
     line(i) = records(i)%line
     freqkHz(i) = records(i)%frequency_khz
     nage = modulo(now - records(i)%utc, 1440)
     iage = nage / nquad
     write(line(i)(79:80),1021) iage
1021 format(i2)
  enddo
  call indexx(freqkHz, nz, indx)

  group_first = 1
  do while (group_first <= nz)
     group_last = group_first
     do while (group_last < nz)
        if (freqkHz(indx(group_last+1)) - freqkHz(indx(group_last)) > 2.0*ftol) exit
        group_last = group_last + 1
     enddo

     group_size = group_last - group_first + 1
     call indexx(real(indx(group_first:group_last)), group_size, group_order)
     do i = 1, group_size
        j = indx(group_first + group_order(i) - 1)
        out = line(j)(1:13)//line(j)(28:31)//line(j)(39:45)// &
              line(j)(35:38)//line(j)(46:80)
        if (out(6:8) == '   ') cycle
        if (out(19:22) == out0(19:22) .and. out(31:55) == out0(31:55) .and. &
            out(6:8) == out0(6:8)) cycle
        livecq2 = line(j)
        livecq3 = out(1:61)//' '//livecq2(23:27)//' '//livecq2(79:83)
        write(linenew,'("@",A)') trim(livecq3)
        call write_stdout(trim(linenew)//new_line('a'))
        out0 = out
     enddo
     group_first = group_last + 1
  enddo

  nc = 0
  do i = nz, 1, -1
     out = line(i)(1:13)//line(i)(28:31)//line(i)(39:45)// &
           line(i)(35:38)//line(i)(46:80)
     if (out(6:8) == '   ') cycle
     i1 = index(out(31:), ' ')
     if (i1 == 0) cycle
     callsign = out(i1+31:)
     i2 = index(callsign, ' ')
     if (i2 > 1) callsign(i2:) = ' '
     len = i2 - 1
     if (len < 0) len = 6
     if (len < 3 .or. callsign == ' ') cycle
     if (any(seen_calls(1:nc) == callsign)) cycle
     nc = nc + 1
     seen_calls(nc) = callsign
     freqcall(nc) = out(6:8)//line(i)(9:13)//' '//callsign//line(i)(79:81)//line(i)(40:43)
     call_freqkHz(nc) = freqkHz(i)
  enddo
  if (nc > 0) call indexx(call_freqkHz, nc, call_order)
  do i = 1, nc
     write(linenew2,'("&",A)') trim(freqcall(call_order(i)))
     call write_stdout(trim(linenew2)//new_line('a'))
  enddo

end subroutine display

end module display_mod
