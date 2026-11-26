! Stand-in for the decoder's stdout channel: summarizes the "@" (Messages)
! and "&" (Band Map) lines one display() call sends to the GUI.
module stdout_channel_mod
  implicit none
  integer :: shown = 0, shown_at_utc = 0, want_utc = -1, oldest_utc = 9999
  integer :: bandmap_entries = 0, display_begins = 0
  integer :: first_shown_utc = -1, second_shown_utc = -1
  character(len=3) :: first_bandmap_frequency = ' '
  ! Mode character (# JT65, : Q65) of each of the first 8 Band Map entries.
  character(len=8) :: bandmap_modes = ' '
  character(len=4) :: first_bandmap_utc = ' '
  character(len=12) :: first_bandmap_call = ' '
contains
  subroutine write_stdout(line)
    character(len=*), intent(in) :: line
    integer :: utc
    if (index(line, '<Map65DisplayBegin>') == 1) display_begins = display_begins + 1
    if (line(1:1) == '&' .and. len_trim(line) > 2) then
      bandmap_entries = bandmap_entries + 1
      if (bandmap_entries == 1) first_bandmap_frequency = line(2:4)
      ! "&" + kHz(3) + ndf(5) + " " + callsign(12) + age(2) + mode(1) + UTC(4)
      if (bandmap_entries == 1 .and. len(line) >= 22) first_bandmap_call = line(11:22)
      if (bandmap_entries <= 8 .and. len(line) >= 25) &
        bandmap_modes(bandmap_entries:bandmap_entries) = line(25:25)
      if (bandmap_entries == 1 .and. len(line) >= 29) first_bandmap_utc = line(26:29)
    endif
    if (line(1:1) /= '@') return
    shown = shown + 1
    read(line(20:23), '(i4)') utc         ! hhmm column of the "@" line
    if (shown == 1) first_shown_utc = utc
    if (shown == 2) second_shown_utc = utc
    if (utc == want_utc) shown_at_utc = shown_at_utc + 1
    oldest_utc = min(oldest_utc, utc)
  end subroutine
end module
