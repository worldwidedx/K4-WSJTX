module fst4_host_support
   use fst4_decode, only: fst4_decoder,fst4_options
   use prog_args, only: data_dir
   use fftw3, only: FFTW_ESTIMATE,FFTW_ESTIMATE_PATIENT,FFTW_MEASURE,FFTW_PATIENT,FFTW_EXHAUSTIVE
   implicit none
contains
   subroutine prepare_fst4_host(decoder,options)
      class(fst4_decoder), intent(inout) :: decoder
      type(fst4_options), intent(inout) :: options
      logical exists
      integer unit,ios,i
      integer npatience,nthreads
      common/patience/npatience,nthreads
      options%fft_flags=FFTW_ESTIMATE
      select case(npatience)
      case(1)
         options%fft_flags=FFTW_ESTIMATE_PATIENT
      case(2)
         options%fft_flags=FFTW_MEASURE
      case(3)
         options%fft_flags=FFTW_PATIENT
      case(4)
         options%fft_flags=FFTW_EXHAUSTIVE
      end select
      inquire(file='plotspec',exist=options%measure_doppler)
      inquire(file=trim(data_dir)//'/decdata',exist=exists)
      nullify(decoder%diagnostic,decoder%spectrum_sink)
      if(exists) decoder%diagnostic=>write_fst4_diagnostic
      if(options%measure_doppler) decoder%spectrum_sink=>write_fst4_spectrum
      if(decoder%host_history_loaded) return
      decoder%host_history_loaded=.true.
      inquire(file=trim(data_dir)//'/fst4w_calls.txt',exist=exists)
      if(.not.exists) return
      open(newunit=unit,file=trim(data_dir)//'/fst4w_calls.txt',status='old',iostat=ios)
      if(ios/=0) return
      decoder%nwcalls=0
      do i=1,100
         read(unit,'(a)',iostat=ios) decoder%wcalls(i)
         if(ios/=0) exit
         decoder%wcalls(i)=adjustl(decoder%wcalls(i))
         if(len_trim(decoder%wcalls(i))==0) exit
         decoder%nwcalls=i
      enddo
      close(unit)
   end subroutine

   subroutine finish_fst4_host(decoder)
      class(fst4_decoder), intent(inout) :: decoder
      integer unit,ios,i
      if(decoder%history_dirty) then
         open(newunit=unit,file=trim(data_dir)//'/fst4w_calls.txt',status='replace',iostat=ios)
         if(ios==0) then
            do i=1,decoder%nwcalls
               write(unit,'(a20)') trim(decoder%wcalls(i))
            enddo
            close(unit)
            decoder%history_dirty=.false.
         endif
      endif
      nullify(decoder%diagnostic,decoder%spectrum_sink)
   end subroutine

   subroutine write_fst4_spectrum(spectrum,df,first_frequency)
      real, intent(in) :: spectrum(:),df,first_frequency
      integer :: i
      integer, save :: spectrum_number=0
      do i=1,size(spectrum)
         write(52,'(f12.6,f12.6)') first_frequency+(i-1)*df,spectrum(i)+spectrum_number
      enddo
      spectrum_number=spectrum_number+1
   end subroutine

   subroutine write_fst4_diagnostic(line)
      character(len=*), intent(in) :: line
      integer unit,ios
      open(newunit=unit,file=trim(data_dir)//'/fst4_decodes.dat',status='unknown',position='append',iostat=ios)
      if(ios/=0) return
      write(unit,'(a)') trim(line)
      close(unit)
   end subroutine
end module
