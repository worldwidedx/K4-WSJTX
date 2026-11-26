module ft8var_work_types
  implicit none
  type :: tmpcqdec_struct
    real :: freq,xdt
  end type
  type :: tmpcqsig_struct
    real :: freq,xdt
    complex :: cs(0:7,79)
  end type
  type :: tmpmyc_struct
    real :: freq,xdt
  end type
  type :: tmpmycsig_struct
    real :: freq,xdt
    complex :: cs(0:7,79)
  end type
  type :: tmpqsosig_struct
    real :: freq,xdt
    complex :: cs(0:7,79)
  end type
end module ft8var_work_types

module ft8var_codec_context
  use packjt77, only: pack77_state,pack77_for_state,unpack77_configured_for_state, &
       pack77_options,unpack77_options
  use ft8_codec_context, only: get_ft8_codec_state
  use ft8_mod1, only: icos7,graymap
  implicit none
  private
  public :: genft8var_for_state,genft8sdvar_for_state,ft8var_tones_from_77bits
  public :: tonesdvar,ft8sd1var,ft8sdvar,ft8svar,ft8mfcqvar

  interface
    subroutine tonesdvar(msgd,lcq,knowledge)
      import pack77_state
      character(len=37) :: msgd
      logical(kind=1) :: lcq
      type(pack77_state), target, optional, intent(inout) :: knowledge
    end subroutine
    subroutine ft8sd1var(s8,itone,msgd,msg37,lft8sd,lcq,knowledge)
      import pack77_state
      real :: s8(0:7,79)
      integer :: itone(79)
      character(len=37) :: msgd,msg37
      logical(kind=1) :: lft8sd,lcq
      type(pack77_state), target, optional, intent(inout) :: knowledge
    end subroutine
    subroutine ft8sdvar(s8,srr,itone,msgd,msg37,lft8sd,lcq,knowledge)
      import pack77_state
      real :: s8(0:7,79),srr
      integer :: itone(79)
      character(len=37) :: msgd,msg37
      logical(kind=1) :: lft8sd,lcq
      type(pack77_state), target, optional, intent(inout) :: knowledge
    end subroutine
    subroutine ft8svar(s8,srr,itone,msg37,lft8s,nft8rxfsens,stophint,knowledge)
      import pack77_state
      real :: s8(0:7,79),srr
      integer :: itone(79),nft8rxfsens
      character(len=37) :: msg37
      logical(kind=1) :: lft8s,stophint
      type(pack77_state), target, optional, intent(inout) :: knowledge
    end subroutine
    subroutine ft8mfcqvar(s8,itone,msgd,msg37,lft8sd,knowledge)
      import pack77_state
      real :: s8(0:7,79)
      integer :: itone(79)
      character(len=37) :: msgd,msg37
      logical(kind=1) :: lft8sd
      type(pack77_state), target, optional, intent(inout) :: knowledge
    end subroutine
  end interface

contains

  subroutine genft8var_for_state(state,msg,i3,n3,ntxhash,msgsent,msgbits,itone)
    type(pack77_state), target, intent(inout) :: state
    character(len=37), intent(in) :: msg
    integer, intent(inout) :: i3,n3
    integer, intent(in) :: ntxhash
    character(len=37), intent(out) :: msgsent
    integer(kind=1), intent(out) :: msgbits(77)
    integer, intent(out) :: itone(79)
    character(len=77) :: c77
    logical :: unpacked

    call pack77_for_state(state,msg,i3,n3,c77,pack77_options(record_tx_hashes=ntxhash.eq.1))
    call unpack77_configured_for_state(state,c77,0,msgsent,unpacked, &
         unpack77_options(record_hashes=.false.,record_recent_calls=.false.))
    read(c77,'(77i1)',err=10) msgbits
    if(unpacked) then
       call ft8var_tones_from_77bits(msgbits,itone)
       return
    endif
10  msgbits=0
    itone=0
    msgsent='*** bad message ***'
  end subroutine genft8var_for_state

  subroutine genft8sdvar_for_state(state,msg,i3,n3,msgsent,msgbits,itone)
    type(pack77_state), target, intent(inout) :: state
    character(len=37), intent(in) :: msg
    integer, intent(out) :: i3,n3
    character(len=37), intent(out) :: msgsent
    integer(kind=1), intent(out) :: msgbits(77)
    integer, intent(out) :: itone(79)
    character(len=77) :: c77
    logical :: unpacked

    i3=-1
    n3=-1
    call pack77_for_state(state,msg,i3,n3,c77,pack77_options(record_tx_hashes=.false.))
    if(.not.(i3.ge.0 .and. (i3.eq.1 .or. i3.eq.2) .and. index(msg,'<').eq.0)) then
       i3=-1
       n3=-1
    endif
    if(i3.ge.0) then
       call unpack77_configured_for_state(state,c77,0,msgsent,unpacked, &
            unpack77_options(record_hashes=.false.,record_recent_calls=.false.))
       read(c77,'(77i1)',err=20) msgbits
       if(unpacked) then
          call ft8var_tones_from_77bits(msgbits,itone)
          return
       endif
    endif
20  msgbits=0
    itone=0
    msgsent='*** bad message ***'
  end subroutine genft8sdvar_for_state

  subroutine ft8var_tones_from_77bits(msgbits,itone)
    integer(kind=1), intent(in) :: msgbits(77)
    integer, intent(out) :: itone(79)
    integer(kind=1) :: codeword(174)
    integer :: i,j,k,indx

    call encode174_91var(msgbits,codeword)
    itone(1:7)=icos7
    itone(37:43)=icos7
    itone(73:79)=icos7
    k=7
    do j=1,58
       i=3*j-2
       k=k+1
       if(j.eq.30) k=k+7
       indx=codeword(i)*4+codeword(i+1)*2+codeword(i+2)
       itone(k)=graymap(indx)
    enddo
  end subroutine ft8var_tones_from_77bits
end module ft8var_codec_context

subroutine genft8var(msg,i3,n3,ntxhash,msgsent,msgbits,itone)
  use ft8var_codec_context, only: genft8var_for_state
  use ft8_codec_context, only: get_ft8_codec_state
  use packjt77, only: pack77_state
  implicit none
  character(len=37), intent(in) :: msg
  integer, intent(inout) :: i3,n3
  integer, intent(in) :: ntxhash
  character(len=37), intent(out) :: msgsent
  integer(kind=1), intent(out) :: msgbits(77)
  integer, intent(out) :: itone(79)
  type(pack77_state), pointer :: state
  state => get_ft8_codec_state()
  call genft8var_for_state(state,msg,i3,n3,ntxhash,msgsent,msgbits,itone)
end subroutine genft8var

subroutine get_tones_from_77bits(msgbits,itone)
  use ft8var_codec_context, only: ft8var_tones_from_77bits
  implicit none
  integer(kind=1), intent(in) :: msgbits(77)
  integer, intent(out) :: itone(79)
  call ft8var_tones_from_77bits(msgbits,itone)
end subroutine get_tones_from_77bits
