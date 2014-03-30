cmake_minimum_required(VERSION 2.6)

option(DASP_HAS_CANDY "Use DanvilSimpleEngine to enable 3D rendering" OFF)
option(DASP_HAS_OPENNI "Use OpenNI for Kinect live mode" OFF)

option(USE_SOLVER_MAGMA "Use CUDA magma for spectral solving" ON)
option(USE_SOLVER_IETL "Use IETL sparse eigensolver" ON)

set(OPENNI_INCLUDE_DIR "/home/david/Programs/RGBD/OpenNI/Include" CACHE STRING "OpenNI include directory")
set(EIGEN3_INCLUDE_DIR "/usr/include/eigen3" CACHE STRING "Eigen3 include directory")
set(SLIMAGE_INCLUDE_DIR "slimage" CACHE STRING "Slimage include directory")
set(DT_DIR "/home/david/git/DanvilTools" CACHE STRING "Danvil tools main directory")
set(CANDY_DIR "/home/david/git/candy" CACHE STRING "Danvil candy 3D engine main directory")

add_definitions(-std=c++0x -DBOOST_DISABLE_ASSERTS)

if (DASP_HAS_CANDY)
	link_directories(/home/david/build/candy/libcandy) # FIXME
endif (DASP_HAS_CANDY)

if (USE_SOLVER_MAGMA)
	link_directories(/opt/cuda/lib64)
    include_directories(
        /usr/include/magma
        /opt/cuda/include
    )
endif (USE_SOLVER_MAGMA)

include_directories(
	${EIGEN3_INCLUDE_DIR}
	.
	${SLIMAGE_INCLUDE_DIR}
)

project(dasp)

add_subdirectory(density)
add_subdirectory(density_cmd)

add_subdirectory(pds)
add_subdirectory(pds_cmd)

add_subdirectory(asp)
add_subdirectory(asp_cmd)

add_subdirectory(graphseg)

add_subdirectory(rgbd)

add_subdirectory(libdasp)
add_subdirectory(dasp_cmd)
add_subdirectory(dasp_eval)
add_subdirectory(dasp_gui)
add_subdirectory(dasp_density)
add_subdirectory(normals_cmd)
