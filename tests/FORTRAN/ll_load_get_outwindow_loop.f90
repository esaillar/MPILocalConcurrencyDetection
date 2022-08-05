PROGRAM main
include 'mpif.h'

   INTEGER, ALLOCATABLE, SAVE, DIMENSION(:)    :: window_buffer
   INTEGER                                     :: myrank, comm_size, ierr
   REAL                                        :: check

   INTEGER (KIND=MPI_ADDRESS_KIND)             :: disp_real, window
   INTEGER (KIND=MPI_ADDRESS_KIND)             :: lowerbound, size_window, realextent, deplacement

   deplacement = 0

   call MPI_init_thread( MPI_THREAD_MULTIPLE, provided, ierr )

   CALL mpi_comm_rank( MPI_COMM_WORLD, myrank, ierr )
   CALL mpi_comm_size( MPI_COMM_WORLD, comm_size, ierr )

   IF (comm_size .ne. 3) THEN
    print *, "This application is meant to be run with 3 MPI processes, not ",comm_size
    call MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE, ierr);
   END IF

   my_value = 12345

   ALLOCATE(window_buffer(100))
   DO i = 1, 100
      window_buffer(i) = 0
   END DO

   call MPI_TYPE_GET_EXTENT(MPI_real, lowerbound, realextent, ierr)
   IF( ierr /= MPI_SUCCESS) THEN 
      print *, "MPI_ERROR line:", __LINE__ , ", rank: ", myrank
   END IF

   disp_real = realextent
   size_window = 100 * realextent

   call MPI_WIN_CREATE(window_buffer(1), size_window, disp_real, MPI_INFO_NULL, MPI_COMM_WORLD, window, ierr)
   IF( ierr /= MPI_SUCCESS) THEN 
      print *, "MPI_ERROR line:", __LINE__ , ", rank: ", myrank
   END IF

   call MPI_Win_fence(0, window, ierr)
   IF( ierr /= MPI_SUCCESS) THEN 
      print *, "MPI_ERROR line:", __LINE__ , ", rank: ", myrank
   END IF

  if (myrank == 0) then
      check = 0.0
      DO WHILE(check == 0.0)
        CALL mpi_get(check, 1, mpi_real, myrank, deplacement, 1, mpi_real, window, ierr)
        ! print *, "Put from ",myrank," to 1"
        IF( ierr /= MPI_SUCCESS) THEN 
                print *, "MPI_ERROR line:", __LINE__ , ", rank: ", myrank
        END IF
      END DO
  end if

  call mpi_win_fence(0, window, ierr)

  call mpi_win_free(window, ierr)
 
  CALL mpi_finalize(ierr)

END PROGRAM main
