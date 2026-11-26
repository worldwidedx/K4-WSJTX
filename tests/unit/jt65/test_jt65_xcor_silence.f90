program test_jt65_xcor_silence

  use jt65_mod, only: jt65_work, ensure_jt65_workspace
  implicit none

  integer, parameter :: lag1 = -32, lag2 = 82, nhmax = 3413, nsmax = 552
  real :: ccf(lag1:lag2), ccf0, flip
  integer :: lagpk


  call setup65
  call ensure_jt65_workspace()
  allocate(jt65_work%spectra(nsmax,nhmax))
  jt65_work%spectra = 0.0
  lagpk = huge(0)

  call xcor(1,nsmax,126,lag1,lag2,ccf,ccf0,lagpk,flip,0.0,0)

  call require(lagpk == lag1, 'silence returns the first valid lag')
  call require(all(ccf == 0.0), 'silence produces zero correlation')
  call require(ccf0 == 0.0, 'silence produces a zero peak')
  call require(flip == 1.0, 'silence keeps normal polarity')

contains

  subroutine require(condition, description)
    logical, intent(in) :: condition
    character(len=*), intent(in) :: description

    if (.not. condition) then
       print '(a)', 'FAIL: '//description
       error stop 1
    end if
  end subroutine require

end program test_jt65_xcor_silence
