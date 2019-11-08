
set(OPENCV_ROOT "${CMAKE_SOURCE_DIR}/opencv")
set(MOONVDEC_ROOT "${CMAKE_SOURCE_DIR}/moonvdec")
set(SATLIB_ROOT "${CMAKE_SOURCE_DIR}/starburst")
set(NVCODEC_ROOT "${CMAKE_SOURCE_DIR}/NvCodec")


set(OPENCV_INCLUDE_DIR "${OPENCV_ROOT}/include")
set(MOONVDEC_INCLUDE_DIR "${MOONVDEC_ROOT}/include")
set(SATLIB_INCLUDE_DIR "${SATLIB_ROOT}/include")
set(NVCODEC_INCLUDE_DIRS
    "${NVCODEC_ROOT}/include" 
    "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v10.1\\include"
)


find_library(OPENCV_LIBRARIES
    NAMES opencv_world401
    HINTS "${OPENCV_ROOT}/lib/${LIBRARY_DIR}"
)
find_library(MOONVDEC_LIBRARIES
    NAMES moonvdec
    HINTS "${MOONVDEC_ROOT}/lib/${LIBRARY_DIR}"
)
find_library(SATLIB_LIBRARIES
    NAMES satellite
    HINTS "${SATLIB_ROOT}/lib/${LIBRARY_DIR}"
)
find_library(CUDA_LIBRARY
    NAMES cuda
    HINTS "${NVCODEC_ROOT}/lib/${LIBRARY_DIR}"
)
find_library(NVCUVID_LIBRARY
    NAMES nvcuvid
    HINTS "${NVCODEC_ROOT}/lib/${LIBRARY_DIR}"
)
set(NVCODEC_LIBRARIES ${CUDA_LIBRARY} ${NVCUVID_LIBRARY})


file(GLOB_RECURSE OPENCV_BINARIES "${OPENCV_ROOT}/lib/${LIBRARY_DIR}/*.dll")
file(GLOB_RECURSE MOONVDEC_BINARIES "${MOONVDEC_ROOT}/lib/${LIBRARY_DIR}/*.dll")
file(GLOB_RECURSE SATLIB_BINARIES "${SATLIB_ROOT}/lib/${LIBRARY_DIR}/*.dll")
