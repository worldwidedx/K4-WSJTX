subroutine ft8apset(mycall12,hiscall12,ncontest,apsym,aph10,knowledge)
  use packjt77
  character*77 c77
  character*37 msg,msgchk
  character*12 mycall12,hiscall12,hiscall
  character*13 hc13
  character*10 c10
  integer apsym(58),aph10(10)
  logical nohiscall,unpk77_success,std
  type(pack77_state), target, optional, intent(inout) :: knowledge
  type(pack77_state), pointer :: codec

  if(present(knowledge)) codec => knowledge

  apsym=0
  apsym(1)=99
  apsym(30)=99
  aph10=0
  aph10(1)=99
  if(len(trim(mycall12)).lt.3) return 

  nohiscall=.false. 
  hiscall=hiscall12 
  if(len(trim(hiscall)).lt.3) then
     hiscall='KA1ABC'                   !Use a dummy hiscall
     nohiscall=.true.
  else
     hc13=hiscall
     n10=0
     n12=0
     n22=0
     if(present(knowledge)) then
        call save_hash_call_for_state(codec,hc13,n10,n12,n22)
     else
        call save_hash_call(hc13,n10,n12,n22)
     endif
     write(c10,'(b10.10)') iand(n10,Z'3FF') 
     read(c10,'(10i1.1)',err=1) aph10
     aph10=2*aph10-1
  endif

! Encode a dummy standard message: i3=1, 28 1 28 1 1 15
!
  msg=trim(mycall12)//' '//trim(hiscall)//' RRR'
  call stdcall(mycall12,std)
  if(.not.std) msg='<'//trim(mycall12)//'> '//trim(hiscall)//' RRR'
  i3=0
  n3=0
  if(present(knowledge)) then
     call pack77_for_state(codec,msg,i3,n3,c77)
     call unpack77_for_state(codec,c77,1,msgchk,unpk77_success)
  else
     call pack77(msg,i3,n3,c77)
     call unpack77(c77,1,msgchk,unpk77_success)
  endif
  if(ncontest.eq.7.and. (i3.ne.1 .or. .not.unpk77_success)) return
  if(ncontest.le.5.and. (i3.ne.1 .or. msg.ne.msgchk .or. .not.unpk77_success)) return 

  read(c77,'(58i1)',err=2) apsym(1:58)
  apsym=2*apsym-1
  if(nohiscall) then
    apsym(30)=99
    aph10(1)=99 
  endif
  return

1 aph10=0
  aph10(1)=99
  return
2 apsym=0
  apsym(1)=99
  apsym(30)=99
  return

end subroutine ft8apset
