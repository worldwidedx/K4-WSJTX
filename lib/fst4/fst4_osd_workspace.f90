module fst4_osd_workspace
  implicit none
  private
  public :: fst4_osd_workspace_type
  type generator_cache
     integer(kind=1), allocatable :: matrix(:,:)
  end type
  type fst4_osd_workspace_type
     type(generator_cache) :: generator(0:101)
     integer(kind=1), allocatable :: matrix(:),transpose_matrix(:),vectors(:,:)
     integer, allocatable :: box_indices(:,:),box_first(:),box_next(:)
     integer :: last_pattern=-1,next_index=-1
   contains
     procedure :: ensure
     procedure :: ensure_boxes
     procedure :: destroy
  end type
contains
  subroutine ensure(this)
     class(fst4_osd_workspace_type), intent(inout) :: this
     if(allocated(this%matrix)) return
     allocate(this%matrix(240*101),this%transpose_matrix(240*101),this%vectors(240,12))
  end subroutine
  subroutine ensure_boxes(this)
     class(fst4_osd_workspace_type), intent(inout) :: this
     if(allocated(this%box_indices)) return
     allocate(this%box_indices(5000,2),this%box_first(0:525000),this%box_next(5000))
  end subroutine
  subroutine destroy(this)
     class(fst4_osd_workspace_type), intent(inout) :: this
     integer i
     do i=lbound(this%generator,1),ubound(this%generator,1)
        if(allocated(this%generator(i)%matrix)) deallocate(this%generator(i)%matrix)
     enddo
     if(allocated(this%matrix)) deallocate(this%matrix,this%transpose_matrix,this%vectors)
     if(allocated(this%box_indices)) deallocate(this%box_indices,this%box_first,this%box_next)
     this%last_pattern=-1
     this%next_index=-1
  end subroutine
end module
