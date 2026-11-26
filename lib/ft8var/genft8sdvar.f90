
subroutine genft8sdvar(msg,i3,n3,msgsent,msgbits,itone)
  use ft8var_codec_context, only: genft8sdvar_for_state
  use ft8_codec_context, only: get_ft8_codec_state
  use packjt77, only: pack77_state
  implicit none
  character(len=37), intent(in) :: msg
  integer, intent(out) :: i3,n3,itone(79)
  character(len=37), intent(out) :: msgsent
  integer(kind=1), intent(out) :: msgbits(77)
  type(pack77_state), pointer :: state
  state => get_ft8_codec_state()
  call genft8sdvar_for_state(state,msg,i3,n3,msgsent,msgbits,itone)
end subroutine genft8sdvar
