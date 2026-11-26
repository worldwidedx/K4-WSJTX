subroutine tone8myc(knowledge)

  use packjt77, only: pack77_state
  use ft8_codec_context, only: get_ft8_codec_state
  use ft8var_codec_context, only: genft8var_for_state

  use ft8_mod1, only : idtonemyc,mycall
  character msg37*37,msgsent37*37
  integer itone(79)
  integer*1 msgbits(77)
  type(pack77_state), target, optional, intent(inout) :: knowledge
  type(pack77_state), pointer :: codec

  codec => get_ft8_codec_state()
  if(present(knowledge)) codec => knowledge
  msg37=''; msg37=trim(mycall)//' AA1AAA FN25'
  i3=-1; n3=-1
  call genft8var_for_state(codec,msg37,i3,n3,0,msgsent37,msgbits,itone)
  idtonemyc(1:29)=itone(8:36); idtonemyc(30:58)=itone(44:72)
 
  return
end subroutine tone8myc
