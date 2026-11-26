module sfox_unpack_module
  use iso_c_binding, only: c_ptr, c_null_ptr
  use decoder_engine_types, only: superfox_observation, sf_kind_cq, sf_kind_exchange, &
       sf_kind_free_text, sf_kind_verification
  use packjt77, only: pack77_state
  implicit none
  private
  public :: sfox_unpack_for_state, superfox_callback, sfox_render_legacy
  abstract interface
     subroutine superfox_callback(user_context,record)
       import superfox_observation, c_ptr
       type(c_ptr), intent(in) :: user_context
       type(superfox_observation), intent(in) :: record
     end subroutine superfox_callback
  end interface
contains
subroutine sfox_unpack_for_state(knowledge,callback,user_context,nutc,x,nsnr,f0,dt0,foxcall,notp)

  use packjt77
  implicit real(a-h,o-z)
  implicit integer(i-n)
  type(pack77_state), optional, intent(inout) :: knowledge
  procedure(superfox_callback) :: callback
  type(c_ptr), intent(in) :: user_context
  integer child_index
  parameter (NQU1RKS=203514677)
  integer*1 x(0:49)
  integer*8 n58
  logical success
  character*336 msgbits
  character*22 msg(10)
  character*13 foxcall,c13
  character*10 ssignature
  character*4 crpt(5),grid4
  character*26 freeTextMsg
  character*38 c
  logical use_otp
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/

  child_index=0
  ncq=0
  if (notp.eq.0) then
     use_otp = .FALSE.
  else
     use_otp = .TRUE.
  endif
  write(msgbits,1000) x(0:46)
1000 format(47b7.7)
  read(msgbits(327:329),'(b3)') i3            !Message type
  read(msgbits(1:28),'(b28)') n28           !Standard Fox call
  if(present(knowledge)) then
     call unpack28_for_state(knowledge,n28,foxcall,success)
  else
     call unpack28(n28,foxcall,success)
  endif

  if(i3.eq.1) then
!     Type i3=1 is documented for a compound-Fox c58 layout, but the
!     current transmitter does not emit it and this decoder does not
!     implement the c58 field offsets.  Do not render hound messages with
!     a mis-decoded c28 Fox call.
     go to 900
  else if(i3.eq.2) then                       !Up to 4 Hound calls and free text
     call unpacktext77(msgbits(161:231),freeTextMsg(1:13))
     call unpacktext77(msgbits(232:302),freeTextMsg(14:26))
     do i=26,1,-1
        if(freeTextMsg(i:i).ne.'.') exit
        freeTextMsg(i:i)=' '
     enddo
     call emit(sf_kind_free_text,freeTextMsg)
  else if(i3.eq.3) then                       !CQ FoxCall Grid     
     read(msgbits(1:58),'(b58)') n58          !FoxCall
     do i=11,1,-1
        j=mod(n58,38)+1
        foxcall(i:i)=c(j:j)
        n58=n58/38
     enddo
     foxcall(12:13)='  '
     read(msgbits(59:73),'(b15)') n15
     call unpackgrid(n15,grid4)
     msg(1)='CQ '//trim(foxcall)//' '//grid4
     call emit(sf_kind_cq,trim(msg(1)))
     allz=1
     do i=0,6
        read(msgbits(74+32*i:105+32*i),'(b32)') n32
        if(n32.ne.NQU1RKS) allz=0
     enddo
     if(allz.eq.1) go to 100
     call unpacktext77(msgbits(74:144),freeTextMsg(1:13))
     call unpacktext77(msgbits(145:215),freeTextMsg(14:26))
     do i=26,1,-1
        if(freeTextMsg(i:i).ne.'.') exit
        freeTextMsg(i:i)=' '
     enddo
     if(len(trim(freeTextMsg)).gt.0) call emit(sf_kind_free_text,freeTextMsg)
     go to 100
  endif

  j=281
  iz=4                                         !Max number of reports
  if(i3.eq.2) j=141
  do i=1,iz                                    !Extract the reports
     read(msgbits(j:j+4),'(b5)') n
     if(n.eq.31) then
        crpt(i)='RR73'
     else
        write(crpt(i),1006) n-18
1006    format(i3.2)
        if(crpt(i)(1:1).eq.' ') crpt(i)(1:1)='+'
     endif
     j=j+5
  enddo

! Unpack Hound callsigns and format user-level messages:
  iz=9                                          !Max number of hound calls
  if(i3.eq.2 .or. i3.eq.3) iz=4
  do i=1,iz
     j=28*i + 1
     read(msgbits(j:j+27),'(b28)') n28
     if(present(knowledge)) then
        call unpack28_for_state(knowledge,n28,c13,success)
     else
        call unpack28(n28,c13,success)
     endif
     if(n28.eq.0 .or. n28.eq.NQU1RKS) cycle 
     msg(i)=trim(c13)//' '//trim(foxcall)
     if(msg(i)(1:3).eq.'CQ ') then
        ncq=ncq+1
     else
        if(i3.eq.2) then
           msg(i)=trim(msg(i))//' '//crpt(i)
        else
           if(i.le.5) msg(i)=trim(msg(i))//' RR73'
           if(i.gt.5) msg(i)=trim(msg(i))//' '//crpt(i-5)
        endif
     endif
     if(ncq.le.1 .or. msg(i)(1:3).ne.'CQ ') then
        if(msg(i)(1:3).eq.'CQ ') then
           call emit(sf_kind_cq,trim(msg(i)))
        else
           call emit(sf_kind_exchange,trim(msg(i)))
        endif
     endif
  enddo

  if(msgbits(306:306).eq.'1' .and. ncq.lt.1) then
     call emit(sf_kind_cq,'CQ '//foxcall)
  endif

100 read(msgbits(307:326),'(b20)') notp
  if (use_otp) then
      write(ssignature,'(I6.6)') notp
      call emit(sf_kind_verification,'$VERIFY$ '//trim(foxcall)//' '//trim(ssignature))
   endif
900 return
contains
  subroutine emit(kind,text)
    integer, intent(in) :: kind
    character(len=*), intent(in) :: text
    type(superfox_observation) :: record

    child_index=child_index+1
    record%symbols=x
    record%kind=kind
    record%child_index=child_index
    record%frequency_hz=f0
    record%dt_seconds=dt0
    record%snr_db=nsnr
    record%message=text
    write(record%legacy_line,'(i6.6,i4,f5.1,i5,1x,"~",2x,a)') nutc,nsnr,dt0,nint(f0),text
    record%legacy_line_length=24+len(text)
    call callback(user_context,record)
  end subroutine emit
end subroutine sfox_unpack_for_state

subroutine sfox_render_legacy(user_context,record)
  type(c_ptr), intent(in) :: user_context
  type(superfox_observation), intent(in) :: record
  write(*,'(a)') record%legacy_line(:record%legacy_line_length)
end subroutine sfox_render_legacy
end module sfox_unpack_module

subroutine sfox_unpack(nutc,x,nsnr,f0,dt0,foxcall,notp)
  use iso_c_binding, only: c_null_ptr
  use sfox_unpack_module, only: sfox_unpack_for_state, sfox_render_legacy
  implicit none
  integer nutc,nsnr,notp
  integer*1 x(0:49)
  real f0,dt0
  character*13 foxcall

  call sfox_unpack_for_state(callback=sfox_render_legacy,user_context=c_null_ptr,nutc=nutc,x=x,nsnr=nsnr, &
       f0=f0,dt0=dt0,foxcall=foxcall,notp=notp)
end subroutine sfox_unpack
