module ft4_codec
  use packjt77, only: pack77_state,pack77_legacy_truncating_fallback_for_state,unpack77_for_state
  implicit none
  private
  public :: genft4_for_state,ft4_tones_from_77bits,ft4_scramble

  integer(kind=1), parameter :: rvec(77)=[ &
       0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0, &
       1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1, &
       0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1]

contains

  subroutine genft4_for_state(state,msg0,ichk,msgsent,msgbits,i4tone)
    type(pack77_state), target, intent(inout) :: state
    character(len=37), intent(in) :: msg0
    integer, intent(in) :: ichk
    character(len=37), intent(out) :: msgsent
    integer(kind=1), intent(inout) :: msgbits(77)
    integer, intent(inout) :: i4tone(103)
    character(len=37) :: message
    character(len=77) :: c77
    integer :: i,i3,n3
    logical :: unpacked

    message=msg0
    i=index(message,char(0))
    if(i>0) message(i:)=' '
    message=adjustl(message)
    i3=-1
    n3=-1
    c77=' '
    unpacked=.false.
    call pack77_legacy_truncating_fallback_for_state(state,message,i3,n3,c77)
    if(i3<0.or.n3<0) go to 10
    call unpack77_for_state(state,c77,0,msgsent,unpacked)
    if(ichk==1) return
    read(c77,'(77i1)',err=10) msgbits
    if(.not.unpacked) go to 10
    call ft4_tones_from_77bits(msgbits,i4tone)
    return
10  msgbits=0
    i4tone=0
    msgsent='*** bad message ***'
  end subroutine

  subroutine ft4_scramble(bits)
    integer(kind=1), intent(inout) :: bits(77)
    bits=mod(bits+rvec,2)
  end subroutine

  subroutine ft4_tones_from_77bits(msgbits,i4tone)
    integer(kind=1), intent(in) :: msgbits(77)
    integer, intent(out) :: i4tone(103)
    integer(kind=1) :: scrambled(77),codeword(174)
    integer :: i,is,itmp(87)
    integer, parameter :: graymap(0:3)=[0,1,3,2]

    scrambled=msgbits
    call ft4_scramble(scrambled)
    call encode174_91(scrambled,codeword)
    do i=1,87
       is=codeword(2*i)+2*codeword(2*i-1)
       itmp(i)=graymap(is)
    enddo
    i4tone(1:4)=[0,1,3,2]
    i4tone(5:33)=itmp(1:29)
    i4tone(34:37)=[1,0,2,3]
    i4tone(38:66)=itmp(30:58)
    i4tone(67:70)=[2,3,1,0]
    i4tone(71:99)=itmp(59:87)
    i4tone(100:103)=[3,2,0,1]
  end subroutine
end module ft4_codec

subroutine genft4(msg0,ichk,msgsent,msgbits,i4tone)
  use decoder_codec_context, only: get_decoder_codec_state
  use ft4_codec, only: genft4_for_state,ft4_scramble
  implicit none
  character(len=37), intent(in) :: msg0
  integer, intent(in) :: ichk
  character(len=37), intent(out) :: msgsent
  integer(kind=1), intent(inout) :: msgbits(77)
  integer, intent(inout) :: i4tone(103)

  call genft4_for_state(get_decoder_codec_state(),msg0,ichk,msgsent,msgbits,i4tone)
  ! The legacy encoder returns scrambled bits; engine evidence uses canonical bits.
  if(ichk/=1.and.msgsent/='*** bad message ***') call ft4_scramble(msgbits)
end subroutine genft4

subroutine get_ft4_tones_from_77bits(msgbits,i4tone)
  use ft4_codec, only: ft4_tones_from_77bits,ft4_scramble
  implicit none
  integer(kind=1), intent(inout) :: msgbits(77)
  integer, intent(out) :: i4tone(103)

  call ft4_tones_from_77bits(msgbits,i4tone)
  call ft4_scramble(msgbits)
end subroutine get_ft4_tones_from_77bits
