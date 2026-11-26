module ft8_codec_context
  use decoder_codec_context, only: get_ft8_codec_state => get_decoder_codec_state
  use packjt77, only: pack77_state,pack77_legacy_truncating_fallback_for_state, &
       unpack77_for_state
  implicit none
  private
  public :: get_ft8_codec_state,genft8_for_state,ft8_tones_from_77bits

contains

  subroutine genft8_for_state(state,msg,i3,n3,msgsent,msgbits,itone)
    type(pack77_state), target, intent(inout) :: state
    character(len=37), intent(in) :: msg
    character(len=37), intent(out) :: msgsent
    integer, intent(out) :: i3,n3,itone(79)
    integer(kind=1), intent(out) :: msgbits(77)
    character(len=77) :: c77
    logical :: unpacked

    i3=-1
    n3=-1
    c77=' '
    unpacked=.false.
    call pack77_legacy_truncating_fallback_for_state(state,msg,i3,n3,c77)
    if(i3.ge.0) call unpack77_for_state(state,c77,0,msgsent,unpacked)
    read(c77,'(77i1)',err=10) msgbits
    if(i3.ge.0 .and. unpacked) then
       call ft8_tones_from_77bits(msgbits,itone)
       return
    endif
10  msgbits=0
    itone=0
    msgsent='*** bad message ***'
  end subroutine genft8_for_state

  subroutine ft8_tones_from_77bits(msgbits,itone)
    integer :: KK,ND,NS,NN,NSPS,NZ,NMAX,NFFT1,NH1,NSTEP,NHSYM,NDOWN
    include 'ft8_params.f90'
    integer(kind=1), intent(in) :: msgbits(77)
    integer, intent(out) :: itone(79)
    integer(kind=1) :: codeword(174)
    integer :: i,j,k,indx
    integer, parameter :: icos7(0:6)=[3,1,4,0,6,5,2]
    integer, parameter :: graymap(0:7)=[0,1,3,2,5,6,4,7]

    call encode174_91(msgbits,codeword)
    itone(1:7)=icos7
    itone(37:43)=icos7
    itone(NN-6:NN)=icos7
    k=7
    do j=1,ND
       i=3*j-2
       k=k+1
       if(j.eq.30) k=k+7
       indx=codeword(i)*4+codeword(i+1)*2+codeword(i+2)
       itone(k)=graymap(indx)
    enddo
  end subroutine ft8_tones_from_77bits
end module ft8_codec_context

subroutine genft8(msg,i3,n3,msgsent,msgbits,itone)
  use ft8_codec_context, only: get_ft8_codec_state,genft8_for_state
  use packjt77, only: pack77_state
  implicit none
  character(len=37), intent(in) :: msg
  character(len=37), intent(out) :: msgsent
  integer, intent(out) :: i3,n3,itone(79)
  integer(kind=1), intent(out) :: msgbits(77)
  type(pack77_state), pointer :: state

  state => get_ft8_codec_state()
  call genft8_for_state(state,msg,i3,n3,msgsent,msgbits,itone)
end subroutine genft8

subroutine get_ft8_tones_from_77bits(msgbits,itone)
  use ft8_codec_context, only: ft8_tones_from_77bits
  implicit none
  integer(kind=1), intent(in) :: msgbits(77)
  integer, intent(out) :: itone(79)
  call ft8_tones_from_77bits(msgbits,itone)
end subroutine get_ft8_tones_from_77bits
