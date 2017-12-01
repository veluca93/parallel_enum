#OpenMPI and Mvapich/mpich require different headers
#based on the configuration options return one or the other

def mpi_hdr():
    MPI_LIB_IS_OPENMPI=True
    hdrs = []    
    if MPI_LIB_IS_OPENMPI:
        hdrs = ["mpi.h", "mpi_portable_platform.h"]   #When using OpenMPI
    else:
        hdrs = ["mpi.h",  "mpio.h", "mpicxx.h"]        #When using MVAPICH
    return hdrs

def if_mpi(if_true, if_false = []):
    return select({
        "//third_party/mpi:with_mpi_support": if_true,
        "//conditions:default": if_false
    })

def additional_mpi_lib_defines():
  return select({
      "//third_party/mpi:with_mpi_support": ["PARALLELENUM_USE_MPI"],
      "//conditions:default": [],
  })