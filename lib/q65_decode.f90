module q65_decode

  use q65_workspace, only: q65_workspace_type
  use ft8_decode, only: ft8apset
  use q65, only: q65_state,q65_work,legacy_work,q65_select,q65_initialize_state,q65_release_state,q65_hist2
  use q65_codec
  use packjt77, only: pack77_state,initialize_pack77_state,reset_pack77_state
  use types, only: q3list
  use q65_callers, only: Q65_MAX_CALLERS,q65_record_caller,q65_expire_callers
  use, intrinsic :: iso_c_binding

  private
  public :: q65_decoder,q65_options,q65_decode_callback,q65_diagnostic_callback
  public :: nsnr0,nfreq0,xdt0,msg0,cq0

  type q65_options
    integer :: utc=0,period=60,submode=0,receive_frequency=1500,tolerance=1000
    integer :: low_frequency=200,high_frequency=4000,effort=1,max_drift=0,qso_progress=0,contest=0,now=0
    logical :: repeat=.false.,single_decode=.false.,average=.false.,auto_clear=.false.
    logical :: eme=.false.,pileup=.false.,ap_cq_only=.false.
    character(len=12) :: mycall='',hiscall=''
    character(len=6) :: hisgrid=''
  end type

  integer nsnr0,nfreq0
  real xdt0
  character msg0*37,cq0*3

  type :: q65_decoder
     type(q65_workspace_type) :: workspace
     type(q65_state), pointer :: state=>null()
     type(pack77_state), pointer :: knowledge=>null()
     type(c_ptr) :: codec=c_null_ptr
     integer(c_int64_t) :: input_id=0
     integer :: period=0,submode=-1,caller_count=0,now=0
     logical :: engine_active=.false.,consumed=.false.
     type(q3list), allocatable :: callers(:)
     procedure(q65_diagnostic_callback), pointer, nopass :: diagnostic=>null()
     procedure(q65_decode_callback), pointer :: callback=>null()
   contains

     procedure :: decode
     procedure :: decode_pcm
     procedure :: initialize
     procedure :: release_input
     procedure :: clear_averages
     procedure :: reset
     procedure :: destroy
     procedure :: set_callers
     procedure :: get_callers
     procedure :: get_curves
     final :: finalize
  end type q65_decoder

  abstract interface
     subroutine q65_diagnostic_callback(line)
       character(len=*), intent(in) :: line
     end subroutine

     subroutine q65_decode_callback (this,nutc,snr1,nsnr,dt,freq,    &
          decoded,idec,nused,ntrperiod,iflagdec)
       import q65_decoder
       implicit none
       class(q65_decoder), intent(inout) :: this
       integer, intent(in) :: nutc
       real, intent(in) :: snr1
       integer, intent(in) :: nsnr
       real, intent(in) :: dt
       real, intent(in) :: freq
       character(len=37), intent(in) :: decoded
       integer, intent(in) :: idec
       integer, intent(in) :: nused
       integer, intent(in) :: ntrperiod
       integer, intent(in) :: iflagdec  !Recovered spare 78th bit (see genq65/q65_ap)
     end subroutine q65_decode_callback
  end interface

contains

  subroutine initialize(this)
    class(q65_decoder), target, intent(inout) :: this
    if(.not.associated(this%state)) then
       allocate(this%state)
       call q65_initialize_state(this%state)
    endif
    if(.not.associated(this%knowledge)) then
       allocate(this%knowledge)
       call initialize_pack77_state(this%knowledge)
    endif
    if(.not.allocated(this%callers)) allocate(this%callers(Q65_MAX_CALLERS))
  end subroutine

  subroutine release_input(this)
    class(q65_decoder), target, intent(inout) :: this
    this%input_id=0
    this%consumed=.false.
    if(.not.associated(this%state)) return
    this%state%spectra_valid=.false.
    this%state%analytic_valid=.false.
  end subroutine

  subroutine clear_averages(this)
    class(q65_decoder), target, intent(inout) :: this
    if(.not.associated(this%state)) return
    if(associated(this%state%s1a)) this%state%s1a=0.
    if(associated(this%state%ccf2_avg)) this%state%ccf2_avg=0.
    this%state%navg=0
  end subroutine

  subroutine reset(this)
    class(q65_decoder), target, intent(inout) :: this
    call this%release_input()
    call this%clear_averages()
    this%caller_count=0
    this%period=0
    this%submode=-1
    if(associated(this%state)) then
       this%state%nhist=0
       this%state%curve_dt=0.
       this%state%curve_average_count=0
       if(associated(this%state%ccf2)) this%state%ccf2=0.
    endif
    if(associated(this%knowledge)) call reset_pack77_state(this%knowledge)
  end subroutine

  subroutine destroy(this)
    class(q65_decoder), target, intent(inout) :: this
    if(associated(this%state)) then
       if(associated(q65_work,this%state)) call q65_select(legacy_work)
       call q65_release_state(this%state)
       deallocate(this%state)
       nullify(this%state)
    endif
    if(associated(this%knowledge)) deallocate(this%knowledge)
    nullify(this%knowledge)
    if(c_associated(this%codec)) call q65_codec_destroy(this%codec)
    this%codec=c_null_ptr
    nullify(this%callback,this%diagnostic)
    if(allocated(this%callers)) deallocate(this%callers)
    call this%workspace%destroy()
    this%input_id=0
    this%consumed=.false.
    this%period=0
    this%submode=-1
    this%caller_count=0
    this%engine_active=.false.
  end subroutine

  subroutine finalize(this)
    type(q65_decoder), intent(inout) :: this
    call this%destroy()
  end subroutine

  subroutine set_callers(this,callers,count)
    class(q65_decoder), target, intent(inout) :: this
    type(q3list), intent(in) :: callers(:)
    integer, intent(in) :: count
    call this%initialize()
    this%caller_count=max(0,min(count,size(callers),Q65_MAX_CALLERS))
    this%callers(:this%caller_count)=callers(:this%caller_count)
  end subroutine

  subroutine get_callers(this,callers,count)
    class(q65_decoder), intent(in) :: this
    type(q3list), intent(out) :: callers(:)
    integer, intent(out) :: count
    integer n
    count=this%caller_count
    n=min(count,size(callers))
    if(n>0) callers(:n)=this%callers(:n)
  end subroutine

  subroutine get_curves(this,even,odd,spacing,offset,instant,averaged,count,render_count)
    class(q65_decoder), intent(in) :: this
    integer, intent(out) :: even,odd,count
    integer, optional, intent(out) :: render_count
    real, intent(out) :: spacing,offset
    real, intent(out) :: instant(:),averaged(:)
    integer n
    if(present(render_count)) render_count=0
    even=0
    odd=0
    count=0
    spacing=0.
    offset=0.
    if(.not.associated(this%state)) return
    if(present(render_count)) render_count=this%state%curve_average_count
    even=this%state%navg(0)
    odd=this%state%navg(1)
    spacing=this%state%df
    offset=this%state%curve_dt
    if(.not.associated(this%state%ccf2)) return
    count=size(this%state%ccf2)
    n=min(count,size(instant),size(averaged))
    instant(:n)=this%state%ccf2(:n)
    averaged(:n)=this%state%ccf2_avg(:n)
  end subroutine

  subroutine decode_pcm(this,callback,samples,sample_count,options,input_id,status)
    class(q65_decoder), target, intent(inout) :: this
    procedure(q65_decode_callback) :: callback
    integer(c_int16_t), contiguous, intent(in) :: samples(:)
    integer, intent(in) :: sample_count
    type(q65_options), intent(in) :: options
    integer(c_int64_t), intent(in) :: input_id
    integer, intent(out) :: status
    integer depth,nqf(20),retry(20),navg0,i,npts,nfq,ntol,nqd
    logical fresh,clear,again,single
    character(len=12) mycall,hiscall
    character(len=6) hisgrid
    status=-1
    npts=options%period*12000
    ! Engine input preparation supplies a full period with an internally padded tail.
    if(sample_count<1.or.sample_count>npts.or.size(samples)<npts) return
    call this%initialize()
    if(.not.c_associated(this%codec)) this%codec=q65_codec_create()
    if(.not.c_associated(this%codec)) return
    if(this%period/=options%period.or.this%submode/=options%submode) then
       call this%release_input()
       call this%clear_averages()
       this%period=options%period
       this%submode=options%submode
    endif
    if(this%input_id/=input_id) call this%release_input()
    this%input_id=input_id
    fresh=.not.this%consumed.and..not.options%repeat
    this%now=options%now
    if(options%contest==1) call q65_expire_callers(this%callers,this%caller_count,this%now)
    this%engine_active=.true.
    call select_q65_codec(this%codec)
    depth=options%effort
    if(options%average) depth=ior(depth,16)
    if(options%auto_clear) depth=ior(depth,128)
    clear=.false.
    again=options%repeat
    single=options%single_decode
    nqd=1
    nfq=options%receive_frequency
    ntol=options%tolerance
    mycall=options%mycall
    hiscall=options%hiscall
    hisgrid=options%hisgrid
    ! Legacy Q65 tests only emedelay > 0; its magnitude does not set the search window.
    call decode(this,callback,samples,nqd,options%utc,options%period,options%submode,nfq, &
         ntol,depth,options%low_frequency,options%high_frequency,clear,single,again,options%max_drift, &
         fresh,merge(2.5,0.,options%eme),mycall,hiscall,hisgrid,options%qso_progress,options%contest, &
         options%pileup,options%ap_cq_only,navg0,nqf)
    this%consumed=.true.
    retry=nqf
    if(.not.options%repeat.and.options%contest==1.and.options%period==60.and.options%submode==0) then
       do i=1,20
          if(retry(i)==0) cycle
          hiscall=options%hiscall
          hisgrid=options%hisgrid
          fresh=.false.
          again=.true.
          single=.true.
          depth=ior(depth,3)
          nfq=retry(i)
          ntol=5
          nqd=1
          call decode(this,callback,samples,nqd,options%utc,options%period,options%submode,nfq, &
               ntol,depth,options%low_frequency,options%high_frequency,clear,single,again,options%max_drift, &
               fresh,merge(2.5,0.,options%eme),mycall,hiscall,hisgrid,options%qso_progress,options%contest, &
               options%pileup,options%ap_cq_only,navg0,nqf)
       enddo
    endif
    status=q65_codec_status()
    call select_q65_codec(c_null_ptr)
    call q65_select(legacy_work)
    this%engine_active=.false.
  end subroutine

  subroutine emit_diagnostic(this,line)
    use prog_args, only: data_dir
    class(q65_decoder), target, intent(inout) :: this
    character(len=*), intent(in) :: line
    integer ios
    if(associated(this%diagnostic)) then
       call this%diagnostic(trim(line))
    else if(.not.this%engine_active) then
       open(22,file=trim(data_dir)//'/q65_decodes.txt',status='unknown',position='append',iostat=ios)
       if(ios==0) then
          write(22,'(a)') trim(line)
          close(22)
       endif
    endif
  end subroutine

  subroutine decode(this,callback,iwave,nqd0,nutc,ntrperiod,nsubmode,nfqso,  &
       ntol,ndepth,nfa0,nfb0,lclearave,single_decode,lagain,max_drift0,      &
       lnewdat0,emedelay,mycall,hiscall,hisgrid,nQSOprogress,ncontest,       &
       lq65pileup,lapcqonly,navg0,nqf)

! Top-level routine that organizes the decoding of Q65 signals
! Input:  iwave            Raw data, i*2
!         nutc             UTC for time-tagging the decode
!         ntrperiod        T/R sequence length (s)
!         nsubmode         Tone-spacing indicator, 0-4 for A-E
!         nfqso            Target signal frequency (Hz)
!         ntol             Search range around nfqso (Hz)
!         ndepth           Optional decoding level
!         lclearave        Flag to clear the message-averaging arrays
!         emedelay         Sync search extended to cover EME delays
!         nQSOprogress     Auto-sequencing state for the present QSO
!         ncontest         Supported contest type
!         lq65pileup       Q65 Pileup AP flag policy
!         lapcqonly        Flag to use AP only for CQ calls
! Output: sent to the callback routine for display to user

    use timer_module, only: timer
    use packjt77
    use, intrinsic :: iso_c_binding
    use q65                               !Shared variables
    use prog_args
    use types
    use q65_callers, only: Q65_MAX_CALLERS,q65_load_callers,q65_expire_callers
 
    parameter (NMAX=300*12000)  !Max TRperiod is 300 s

    class(q65_decoder), target, intent(inout) :: this

    procedure(q65_decode_callback) :: callback
    character(len=12) :: mycall, hiscall  !Used for AP decoding
    character(len=6) :: hisgrid
    character*37 decoded                  !Decoded message
    character*37 decodes(100)
    character*77 c77
    character*78 c78
    character*6 cutc
    character c6*6,c4*4,cmode*4
    character*80 fmt
    integer(c_int16_t), intent(in) :: iwave(*)
    real xdtdecodes(100)
    real f0decodes(100)
    integer dat4(13)                      !Decoded message as 12 6-bit integers
    integer dgen(13)
    integer nqf(20)
    integer stageno                       !Added by W3SZ
    integer time,history_status
    integer iflagdec                      !Recovered spare 78th bit
    logical lclearave,lnewdat0,lq65pileup,lapcqonly,unpk77_success
    logical single_decode,lagain
    complex, pointer :: c00(:)
    character(len=512) :: diagnostic_line
    type(q3list), pointer :: callers(:)
    integer :: effort,display_utc


    call this%initialize()
    call q65_select(this%state)
    this%state%legacy_output=.not.this%engine_active
    if(this%engine_active) then
       this%state%knowledge=>this%knowledge
    else
       nullify(this%state%knowledge)
       this%state%spectra_valid=.false.
       this%state%analytic_valid=.false.
    endif
    callers=>this%callers
    nqf=0
    effort=ndepth
    display_utc=nutc
    if(this%engine_active.and.ntrperiod>=60) display_utc=nutc/100

! Start by setting some parameters and allocating storage for large arrays
    call sec0(0,tdecode)
    stageno=0
    ndecodes=0
    decodes=' '
    f0decodes=0.
    xdtdecodes=0.
    nfa=nfa0
    nfb=nfb0
    nqd=nqd0
    lnewdat=lnewdat0
    max_drift=max_drift0
    idec=-1
    idf=0
    idt=0
    nrc=-2
    mode_q65=2**nsubmode
    npts=ntrperiod*12000
    nfft1=ntrperiod*12000
    nfft2=ntrperiod*6000
    npasses=1
    nhist2=this%caller_count
    history_status=0
    if(lagain) effort=ior(effort,3)       !Use 'Deep' for manual Q65 decodes
    if(this%engine_active) then
       this%knowledge%dxcall13=hiscall
       this%knowledge%mycall13=mycall
    else
       dxcall13=hiscall
       mycall13=mycall
    endif
    if(ncontest.eq.1.and..not.this%engine_active) then
! NA VHF, WW-Digi, or ARRL Digi Contest
       call q65_load_callers(trim(data_dir)//'/tsil.3q',callers,nhist2,history_status)
       if(history_status.eq.0) call q65_expire_callers(callers,nhist2,time())
    endif

! Determine the T/R sequence: iseq=0 (even), or iseq=1 (odd)
    n=nutc
    if(.not.this%engine_active.and.ntrperiod.ge.60.and.nutc.le.2359) n=100*n
    write(cutc,'(i6.6)') n
    read(cutc,'(3i2)') ih,im,is
    nsec=3600*ih + 60*im + is
    iseq=mod(nsec/ntrperiod,2)

    if(lclearave) call q65_clravg


    if(lagain) then
       call q65_hist(nfqso,dxcall=hiscall,dxgrid=hisgrid)
    endif

    nsps=1800
    if(ntrperiod.eq.30) then
       nsps=3600
    else if(ntrperiod.eq.60) then
       nsps=7200
    else if(ntrperiod.eq.120) then
       nsps=16000
    else if(ntrperiod.eq.300) then
       nsps=41472
    endif

    baud=12000.0/nsps
    this%callback => callback
    nFadingModel=1

!    ibwa=max(1,int(1.8*log(baud*mode_q65)) + 5)
!### This needs work!
    ibwa=1                          !Q65-60A
    if(mode_q65.eq.2) ibwa=3        !Q65-60B
    if(mode_q65.eq.4) ibwa=8        !Q65-60C
    if(mode_q65.eq.8) ibwa=8        !Q65-60D
    if(mode_q65.eq.16) ibwa=8       !Q65-60E
!###

!    ibwb=min(15,ibwa+4)
    ibwb=min(15,ibwa+6)
    maxiters=40
    if(iand(effort,3).eq.2) maxiters=60
    if(iand(effort,3).eq.3) then
       ibwa=max(1,ibwa-2)
       ibwb=min(15,ibwb+2)
       maxiters=100
    endif

! Generate codewords for full-AP list decoding
    if(ichar(hiscall(1:1)).eq.0) hiscall=' '
    if(ichar(hisgrid(1:1)).eq.0) hisgrid=' '
    ncw=0
    if(nqd.eq.1 .or. lagain .or. ncontest.eq.1) then
       if(ncontest.eq.1) then
          call q65_set_list2(mycall,hiscall,hisgrid,callers,nhist2,   &
               codewords,ncw)
       else
          call q65_set_list(mycall,hiscall,hisgrid,codewords,ncw)
       endif
    endif
    dgen=0
    call q65_enc(dgen,codewords)         !Initialize the Q65 codec
    nused=1
    iavg=0

! W3SZ patch: Initialize AP params here, rather than afer the call to ana64().
    call ft8apset(mycall,hiscall,ncontest,apsym0,aph10,this%state%knowledge) ! Generate ap symbols
    where(apsym0.eq.-1) apsym0=0
    npasses=2
    if(nQSOprogress.eq.5) npasses=3

    call timer('q65_dec0',0)
! Call top-level routine in q65 module: establish sync and try for a
! q3 or q0 decode.
    call q65_dec0(iavg,iwave,ntrperiod,nfqso,ntol,lclearave,  &
         emedelay,xdt,f0,snr1,width,dat4,snr2,idec,stageno)
    call timer('q65_dec0',1)

    if(idec.ge.0) then
       dtdec=xdt                    !We have a q3 or q0 decode at nfqso
       f0dec=f0
       go to 100
    endif

    if(ncontest.eq.1 .and. lagain .and. iand(effort,16).eq.16) go to 50
    if(ncontest.eq.1 .and. lagain .and. iand(effort,16).eq.0) go to 100

! Prepare for a single-period decode with iaptype = 0, 1, 2, or 4
    jpk0=(xdt+1.0)*6000                      !Index of nominal start of signal
    if(ntrperiod.le.30) jpk0=(xdt+0.5)*6000  !For shortest sequences
    if(jpk0.lt.0) jpk0=0
    call q65_analytic(iwave,npts)
    c00=>this%state%analytic          !Convert to complex c00() at 6000 Sa/s
    if(lapcqonly) npasses=1
    iaptype=0
    do ipass=0,npasses                  !Loop over AP passes
       apmask=0                         !Try first with no AP information
       apsymbols=0
       if(ipass.ge.1) then
          ! Subsequent passes use AP information appropiate for nQSOprogress
          call q65_ap(nQSOprogress,ipass,ncontest,lq65pileup,lapcqonly,iaptype, &
               apsym0,apmask1,apsymbols1)
          write(c78,1050) apmask1
1050      format(78i1)
          read(c78,1060) apmask
1060      format(13b6.6)
          write(c78,1050) apsymbols1
          read(c78,1060) apsymbols
       endif

       call timer('q65loop1',0)
       call q65_loops(this%workspace,c00,npts/2,nsps/2,nsubmode,effort,jpk0,   &
            xdt,f0,iaptype,xdt1,f1,snr2,dat4,idec)
       call timer('q65loop1',1)
       if(idec.ge.0) then
          dtdec=xdt1
          f0dec=f1
          go to 100       !Successful decode, we're done
       endif
    enddo  ! ipass

    if(iand(effort,16).eq.0 .or. navg(iseq).lt.2) go to 100

! There was no single-transmission decode. Try for an average 'q3n' decode.
50  iavg=1
    call timer('list_avg',0)
! Call top-level routine in q65 module: establish sync and try for a q3
! decode, this time using the cumulative 's1a' symbol spectra.
    call q65_dec0(iavg,iwave,ntrperiod,nfqso,ntol,lclearave,  &
         emedelay,xdt,f0,snr1,width,dat4,snr2,idec,stageno)
    call timer('list_avg',1)

    if(idec.ge.0) then
       dtdec=xdt               !We have a list-decode result from averaged data
       f0dec=f0
       nused=navg(iseq)
       go to 100
    endif

! There was no 'q3n' decode.  Try for a 'q[0124]n' decode.
! Call top-level routine in q65 module: establish sync and try for a q[012]n
! decode, this time using the cumulative 's1a' symbol spectra.

    call timer('q65_avg ',0)
    iavg=2
    call q65_dec0(iavg,iwave,ntrperiod,nfqso,ntol,lclearave,  &
         emedelay,xdt,f0,snr1,width,dat4,snr2,idec,stageno)
    call timer('q65_avg ',1)
    if(idec.ge.0) then
       dtdec=xdt                          !We have a q[012]n result
       f0dec=f0
       nused=navg(iseq)
    endif

100 if(idec.lt.0 .and. max_drift.eq.50) then
       stageno = 5
       call timer('q65_dec0',0)
       ! Call top-level routine in q65 module: establish sync and try for a
       ! q3 or q0 decode.
       call q65_dec0(iavg,iwave,ntrperiod,nfqso,ntol,lclearave,  &
            emedelay,xdt,f0,snr1,width,dat4,snr2,idec,stageno)
       call timer('q65_dec0',1)
       if(idec.ge.0) then
          dtdec=xdt             !We have a q[012]n result
          f0dec=f0
       endif
    endif                       ! if(idec.lt.0)

    decoded='                                     '
    if(idec.ge.0) then
! idec Meaning
! ------------------------------------------------------
! -1:  No decode
!  0:  Decode without AP information
!  1:  Decode with AP for "CQ        ?   ?"
!  2:  Decode with AP for "MyCall    ?   ?"
!  3:  Decode with AP for "MyCall DxCall ?"

! Unpack decoded message for display to user
       iflagdec=iand(dat4(13),1)     !Recover the spare 78th bit before it's shifted away
       write(c77,1000) dat4(1:12),dat4(13)/2
1000   format(12b6.6,b5.5)
       call q65_unpack(c77,1,decoded,unpk77_success) !Unpack to get decoded
       idupe=0
       do i=1,ndecodes
          if(decodes(i).eq.decoded) idupe=1
       enddo
       if(idupe.eq.0 .and. unpk77_success) then
          ndecodes=min(ndecodes+1,100)
          decodes(ndecodes)=decoded
          f0decodes(ndecodes)=f0dec
          xdtdecodes(ndecodes)=dtdec
          call q65_snr(dat4,dtdec,f0dec,mode_q65,snr2)
          nsnr=nint(snr2)
          call this%callback(nutc,snr1,nsnr,dtdec,f0dec,decoded,    &
               idec,nused,ntrperiod,iflagdec)
          if(ncontest.eq.1) then
             call record_caller(this,nint(f0dec),decoded,callers,nhist2,history_status.eq.0)
          else
             call q65_hist(nint(f0dec),msg0=decoded)
          endif
          if(iand(effort,128).ne.0 .and. .not.lagain .and.      &
               int(abs(f0dec-nfqso)).le.ntol ) call q65_clravg    !AutoClrAvg
          call sec0(1,tdecode)
          if(associated(this%diagnostic).or..not.this%engine_active) then
! Save decoding parameters to q65_decoded.dat, for later analysis.
             write(cmode,'(i3)') ntrperiod
             cmode(4:4)=char(ichar('A')+nsubmode)
             c6=hiscall(1:6)
             if(c6.eq.'      ') c6='<b>   '
             c4=hisgrid(1:4)
             if(c4.eq.'    ') c4='<b> '
             fmt='(i6.4,1x,a4,i5,4i2,8i3,i4,f6.2,f7.1,f6.1,f7.1,f6.2,'//   &
                  '1x,a6,1x,a6,1x,a4,1x,a)'
             if(ntrperiod.le.30) fmt(5:5)='6'
             if(idec.eq.3) nrc=0
             write(diagnostic_line,fmt) display_utc,cmode,nfqso,nQSOprogress,idec,idfbest,idtbest, &
                  ibwa,ibwb,ibw,ndistbest,nused,icand,ncand,nrc,effort,xdt,    &
                  f0,snr2,plog,tdecode,mycall(1:6),c6,c4,trim(decoded)
             call emit_diagnostic(this,diagnostic_line)
          endif
       endif
    endif
    navg0=1000*navg(0) + navg(1)
    if(single_decode .or. lagain) go to 900

    do icand=1,ncand
! Prepare for single-period candidate decodes with iaptype = 0, 1, 2, or 4
       snr1=candidates(icand,1)
       xdt= candidates(icand,2)
       f0 = candidates(icand,3)
       do i=1,ndecodes
          fdiff=f0-f0decodes(i)
          if(fdiff.gt.-baud*mode_q65 .and. fdiff.lt.65*baud*mode_q65) go to 800
       enddo

!###  TEST REGION
       if(ncontest.eq.-1) then
          call timer('q65_dec0',0)
! Call top-level routine in q65 module: establish sync and try for a
! q3 or q0 decode.
          call q65_dec0(iavg,iwave,ntrperiod,nint(f0),ntol,lclearave,  &
               emedelay,xdt,f0,snr1,width,dat4,snr2,idec,stageno)
          call timer('q65_dec0',1)
          if(idec.ge.0) then
             dtdec=xdt                    !We have a q3 or q0 decode at f0
             f0dec=f0
             go to 200
          endif
       endif
!###
       jpk0=(xdt+1.0)*6000                   !Index of nominal start of signal
       if(ntrperiod.le.30) jpk0=(xdt+0.5)*6000  !For shortest sequences
       if(jpk0.lt.0) jpk0=0
       call q65_analytic(iwave,npts)
    c00=>this%state%analytic       !Convert to complex c00() at 6000 Sa/s
       call ft8apset(mycall,hiscall,ncontest,apsym0,aph10,this%state%knowledge) ! Generate ap symbols
       where(apsym0.eq.-1) apsym0=0

       npasses=2
       if(nQSOprogress.eq.5) npasses=3
       if(lapcqonly) npasses=1
       iaptype=0
       do ipass=0,npasses                  !Loop over AP passes
!          write(*,3001) nutc,icand,ipass,f0,xdt,snr1
!3001      format('a',i5.4,2i3,3f7.1)
          apmask=0                         !Try first with no AP information
          apsymbols=0
          if(ipass.ge.1) then
          ! Subsequent passes use AP information appropiate for nQSOprogress
             call q65_ap(nQSOprogress,ipass,ncontest,lq65pileup,lapcqonly,iaptype, &
                  apsym0,apmask1,apsymbols1)
             write(c78,1050) apmask1
             read(c78,1060) apmask
             write(c78,1050) apsymbols1
             read(c78,1060) apsymbols
          endif

          call timer('q65loop2',0)
          call q65_loops(this%workspace,c00,npts/2,nsps/2,nsubmode,effort,jpk0,   &
               xdt,f0,iaptype,xdt1,f1,snr2,dat4,idec)
          call timer('q65loop2',1)
!          write(*,3001) '=e',nfqso,ntol,ndepth,xdt,f0,idec
          if(idec.ge.0) then
             dtdec=xdt1
             f0dec=f1
             go to 200       !Successful decode, we're done
          endif
       enddo  ! ipass

200    decoded='                                     '
       if(idec.ge.0) then
! Unpack decoded message for display to user
          iflagdec=iand(dat4(13),1)  !Recover the spare 78th bit before it's shifted away
          write(c77,1000) dat4(1:12),dat4(13)/2
          call q65_unpack(c77,1,decoded,unpk77_success) !Unpack to get decoded
          idupe=0
          do i=1,ndecodes
             if(decodes(i).eq.decoded) idupe=1
          enddo
          if(idupe.eq.0 .and. unpk77_success) then
             ndecodes=min(ndecodes+1,100)
             decodes(ndecodes)=decoded
             f0decodes(ndecodes)=f0dec
             call q65_snr(dat4,dtdec,f0dec,mode_q65,snr2)
             nsnr=nint(snr2)
             call this%callback(nutc,snr1,nsnr,dtdec,f0dec,decoded,    &
                  idec,nused,ntrperiod,iflagdec)
             if(ncontest.eq.1) then
                call record_caller(this,nint(f0dec),decoded,callers,nhist2,history_status.eq.0)
             else
                call q65_hist(nint(f0dec),msg0=decoded)
             endif
             if(iand(effort,128).ne.0 .and. .not.lagain .and.      &
                  int(abs(f0dec-nfqso)).le.ntol ) call q65_clravg    !AutoClrAvg
             call sec0(1,tdecode)
             if(associated(this%diagnostic).or..not.this%engine_active) then
! Save decoding parameters to q65_decoded.dat, for later analysis.
                write(cmode,'(i3)') ntrperiod
                cmode(4:4)=char(ichar('A')+nsubmode)
                c6=hiscall(1:6)
                if(c6.eq.'      ') c6='<b>   '
                c4=hisgrid(1:4)
                if(c4.eq.'    ') c4='<b> '
                fmt='(i6.4,1x,a4,i5,4i2,8i3,i4,f6.2,f7.1,f6.1,f7.1,f6.2,'//   &
                     '1x,a6,1x,a6,1x,a4,1x,a)'
                if(ntrperiod.le.30) fmt(5:5)='6'
                if(idec.eq.3) nrc=0
                write(diagnostic_line,fmt) display_utc,cmode,nfqso,nQSOprogress,idec,idfbest,    &
                     idtbest,ibwa,ibwb,ibw,ndistbest,nused,icand,ncand,nrc,  &
                     effort,xdt,f0,snr2,plog,tdecode,mycall(1:6),c6,c4,      &
                     trim(decoded)
                call emit_diagnostic(this,diagnostic_line)
             endif
          endif
       endif
800    continue
    enddo  ! icand
    if(iavg.eq.0 .and.navg(iseq).ge.2 .and. iand(effort,16).ne.0) go to 50

900 if(ncontest.ne.1 .or. lagain) go to 999
    if(ntrperiod.ne.60 .or. nsubmode.ne.0) go to 999

! This is first time here, and we're running Q65-60A in NA VHF Contest mode.
! Return a list of potential sync frequencies at which to try q3 decoding.

    k=0
    nqf=0
    bw=baud*mode_q65*65
    do i=1,ncand
!       snr1=candidates(i,1)
!       xdt= candidates(i,2)
       f0 = candidates(i,3)
       do j=1,ndecodes          ! Already decoded one at or near this frequency?
          fj=f0decodes(j)
          if(f0.gt.fj-5.0 .and. f0.lt.fj+bw+5.0) go to 990
       enddo
       k=k+1
       nqf(k)=nint(f0)
990    continue
    enddo

999 this%caller_count=nhist2
    return
  end subroutine decode


  subroutine record_caller(this,frequency,message,callers,count,persist)
    class(q65_decoder), target, intent(inout) :: this
    integer, intent(in) :: frequency
    character(len=*), intent(in) :: message
    type(q3list), intent(inout) :: callers(Q65_MAX_CALLERS)
    integer, intent(inout) :: count
    logical, intent(in) :: persist
    if(this%engine_active) then
       call q65_record_caller(callers,count,frequency,message,this%now)
    else
       call q65_hist2(frequency,message,callers,count,persist)
    endif
  end subroutine

end module q65_decode
