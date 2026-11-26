program test_packjt77_context_state
  use packjt77
  implicit none

  type(pack77_state), target :: first, second
  character(len=13), pointer :: saved_calls(:)
  character(len=77) :: bits
  character(len=37) :: message
  character(len=37) :: input
  character(len=13) :: call,resolved_call
  integer :: i3,n3,status,n10,n12,n22
  logical :: ok

  calls10=''
  calls12=''
  calls22=''
  ihash22=-1
  nzhash=0
  mycall13=''
  dxcall13=''

  call='W7ABC'
  n10=-99
  n12=-99
  n22=-99
  call save_hash_call_for_state(first,call,n10,n12,n22)
  call initialize_pack77_state(second)
  call require(first%calls12(n12).eq.call,'first state records the call')
  call require(second%calls12(n12).eq.'','second state is independent')
  call require(calls12(n12).eq.'','legacy store is independent')

  input='PJ2/W1AW <W7ABC> RR73'
  call pack77_for_state(first,input,i3,n3,bits, &
       pack77_options(record_tx_hashes=.false.),status)
  call require(status.eq.PACK77_STATUS_ENCODED,'explicit encode accepted')
  call require(i3.eq.4.and.n3.eq.0,'explicit encode selected type 4')
  call require(first%nzhash.eq.1.and.second%nzhash.eq.0.and.nzhash.eq.0, &
       'encode without TX recording preserves separate stores')

  call unpack77_for_state(second,bits,1,message,ok)
  call require(ok.and.trim(message).eq.'PJ2/W1AW <...> RR73', &
       'second state initially lacks the hashed call')

  call unpack77_for_state(first,bits,1,message,ok)
  call require(ok.and.trim(message).eq.'PJ2/W1AW <W7ABC> RR73', &
       'first state resolves the hashed call')
  call unpack77_for_state(second,bits,1,message,ok)
  call require(ok.and.trim(message).eq.'PJ2/W1AW <...> RR73', &
       'second state remains unresolved')
  call unpack77(bits,1,message,ok)
  call require(ok.and.trim(message).eq.'PJ2/W1AW <...> RR73', &
       'legacy decode remains unresolved')

  call queue_hash_call_for_thread_for_state(second,call,2)
  call require(second%calls12(n12).eq.'','thread fact waits for fold')
  call fold_queued_calls_for_state(second,2)
  call require(second%calls12(n12).eq.call,'thread fact folds into its state')
  call require(calls12(n12).eq.'','thread fold does not mutate legacy store')

  call prepare_configured_decode_for_state(second,'N0AAA','N0BBB')
  call require(second%mycall13_configured_set,'configured call belongs to state')
  call require(.not.first%mycall13_configured_set, &
       'other state configured call remains clear')
  call require(mycall13.eq.'','legacy configured call remains clear')

  n10=-99
  n12=-99
  n22=-99
  call save_hash_call_for_state(first,'<...>        ',n10,n12,n22)
  call require(n10.eq.-99.and.n12.eq.-99.and.n22.eq.-99, &
       'ignored calls leave hash outputs untouched')

  call pack77_for_state(second,'K1ABC W9XYZ FN42',i3,n3,bits,status=status)
  call require(status.eq.PACK77_STATUS_ENCODED.and.i3.eq.1, &
       'explicit encode accepts an assumed-length message')
  call require(nzhash.eq.0,'explicit encode leaves the legacy store untouched')

  call prepare_configured_decode_for_state(first,'N0AAA','N0BBB')
  call queue_hash_call_for_thread_for_state(first,call,2)
  saved_calls=>first%calls12
  call initialize_pack77_state(first)
  call hash12_for_state(first,ihashcall(call,12),resolved_call)
  call require(trim(resolved_call).eq.'<'//trim(call)//'>','initialization preserves learned calls')
  call require(first%mycall13.eq.'N0AAA'.and.first%mycall13_configured_set, &
       'initialization preserves configured calls')
  call require(first%nqueued_calls_by_thread(2).gt.0,'initialization preserves queued calls')

  call reset_pack77_state(first)
  call require(associated(saved_calls,first%calls12),'reset retains table storage and views')
  call require(all(saved_calls.eq.''),'existing view observes the reset')
  call require(lbound(first%calls10,1).eq.0.and.lbound(first%calls12,1).eq.0, &
       'reset preserves zero-based hash tables')
  call hash12_for_state(first,ihashcall(call,12),resolved_call)
  call require(trim(resolved_call).eq.'<...>','reset forgets learned calls')
  call require(first%nzhash.eq.0.and.all(first%ihash22.eq.-1),'reset clears hash entries')
  call require(first%mycall13.eq.''.and..not.first%mycall13_configured_set, &
       'reset clears configured calls')
  call fold_queued_calls_for_state(first,2)
  call require(all(first%calls12.eq.''),'reset discards queued calls before folding')
  call hash12_for_state(second,ihashcall(call,12),resolved_call)
  call require(trim(resolved_call).eq.'<'//trim(call)//'>','reset leaves other states unchanged')
  call save_hash_call_for_state(first,call,n10,n12,n22)
  call require(saved_calls(n12).eq.call,'reset storage remains usable through existing views')

  print *, 'packjt77 explicit state isolation passed'

contains

  subroutine require(condition,label)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: label

    if(condition) return
    print *, 'FAIL: ',label
    error stop 1
  end subroutine require
end program test_packjt77_context_state
