program test_packjt77_grammar_gate

  use packjt77_grammar, only: pack77_gate_messages_match, &
       pack77_exact_free_text_ok,pack77_free_text_alphabet_ok, &
       pack77_configured_type12_ok,pack77_configured_type4_ok
  implicit none

  integer :: ntests
  character(len=13) :: no_calls(1), staged_calls(2)

  ntests=0
  no_calls=''
  staged_calls=''
  staged_calls(1)='PJ4/K1ABC'
  staged_calls(2)='W3CCX'

  call expect_match('equal messages', &
       '  k1abc   w9xyz   fn42', 'K1ABC W9XYZ FN42', no_calls, 0)
  call expect_match('auto-hash canonicalization', &
       'W1AW/P K1ABC 5A CT', '<W1AW/P> K1ABC 5A CT', no_calls, 0)
  call expect_match('staged placeholder', &
       '<PJ4/K1ABC> W9XYZ RR73', '<...> W9XYZ RR73', staged_calls, 2)

  call expect_no_match('unstaged placeholder', &
       '<PJ4/K1ABC> W9XYZ RR73', '<...> W9XYZ RR73', no_calls, 0)
  call expect_no_match('CQ bracketed C11 mismatch', &
       'CQ <PJ4/K1ABC>', 'CQ PJ4/K1ABC', no_calls, 0)
  call expect_no_match('wrong bracket body', &
       '<K1ABC> W9XYZ RR73', '<W9XYZ> W9XYZ RR73', no_calls, 0)
  call expect_no_match('different token count', &
       'K1ABC W9XYZ FN42', 'K1ABC W9XYZ', no_calls, 0)
  call expect_no_match('changed first call', &
       'K1ABC W9XYZ FN42', 'N1ABC W9XYZ FN42', no_calls, 0)
  call expect_no_match('changed second call', &
       'K1ABC W9XYZ FN42', 'K1ABC W9XYA FN42', no_calls, 0)
  call expect_no_match('changed report', &
       'K1ABC W9XYZ -12', 'K1ABC W9XYZ -13', no_calls, 0)
  call expect_no_match('changed grid', &
       'K1ABC W9XYZ FN42', 'K1ABC W9XYZ FN43', no_calls, 0)
  call expect_no_match('changed serial', &
       'K1ABC FN42 37', 'K1ABC FN42 38', no_calls, 0)

  call expect_text_and_configured_receive_rules()

  write(*,1000) ntests
1000 format('packjt77 grammar gate tests passed: ',i0)

contains

  subroutine expect_text_and_configured_receive_rules()
    character(len=13) :: call_1,call_2
    character(len=37) :: msg

    call expect_rule('empty free text',pack77_exact_free_text_ok(''))
    call expect_rule('13-character free text',pack77_exact_free_text_ok('ABCDEFGHIJKLM'))
    call expect_rule('14-character free text rejected',.not.pack77_exact_free_text_ok('ABCDEFGHIJKLMN'))
    call expect_rule('alphabet independent of length',pack77_free_text_alphabet_ok('ABCDEFGHIJKLMN'))
    call expect_rule('free text punctuation',pack77_exact_free_text_ok('0123 +-./?'))
    call expect_rule('lowercase requires normalization',.not.pack77_exact_free_text_ok('abc'))
    call expect_rule('unsupported free text symbol',.not.pack77_free_text_alphabet_ok('ABC@'))

    call_1='<K1ABC>'
    call_2='W9XYZ/R'
    call expect_rule('configured rover collision rejected', &
         .not.pack77_configured_type12_ok(call_1,call_2,'FN42',0))
    call expect_rule('configured rover acknowledgement allowed', &
         pack77_configured_type12_ok(call_1,call_2,'FN42',1))
    call expect_rule('configured rover RR73 allowed', &
         pack77_configured_type12_ok(call_1,call_2,'RR73',0))

    call_1='CQ'
    call_2='PJ4/K1ABC'
    msg='CQ PJ4/K1ABC'
    call expect_rule('configured compound CQ allowed', &
         pack77_configured_type4_ok(call_1,call_2,msg,0,1,0))
    call_2='12/K1ABC'
    msg='CQ 12/K1ABC'
    call expect_rule('configured two-digit prefix rejected', &
         .not.pack77_configured_type4_ok(call_1,call_2,msg,0,1,0))
  end subroutine expect_text_and_configured_receive_rules

  subroutine expect_rule(label,ok)
    character(len=*), intent(in) :: label
    logical, intent(in) :: ok

    if(.not.ok) then
       write(*,'(a)') trim(label)//' failed'
       error stop 1
    endif
    ntests=ntests+1
  end subroutine expect_rule

  subroutine expect_match(label,input_msg,decoded_msg,staged_calls,nstaged)
    character(len=*), intent(in) :: label,input_msg,decoded_msg
    character(len=13), intent(in) :: staged_calls(:)
    integer, intent(in) :: nstaged

    if(.not.pack77_gate_messages_match(input_msg,decoded_msg,staged_calls, &
         nstaged)) then
       write(*,1010) trim(label), trim(input_msg), trim(decoded_msg)
1010   format(a,' expected match for input "',a,'" decoded "',a,'"')
       error stop 1
    endif
    ntests=ntests+1
  end subroutine expect_match

  subroutine expect_no_match(label,input_msg,decoded_msg,staged_calls,nstaged)
    character(len=*), intent(in) :: label,input_msg,decoded_msg
    character(len=13), intent(in) :: staged_calls(:)
    integer, intent(in) :: nstaged

    if(pack77_gate_messages_match(input_msg,decoded_msg,staged_calls,nstaged)) then
       write(*,1020) trim(label), trim(input_msg), trim(decoded_msg)
1020   format(a,' expected mismatch for input "',a,'" decoded "',a,'"')
       error stop 1
    endif
    ntests=ntests+1
  end subroutine expect_no_match

end program test_packjt77_grammar_gate
