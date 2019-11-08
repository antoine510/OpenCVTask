include_directories("${CMAKE_SOURCE_DIR}/TestClient")

set(HEADER_FILES
    NvEncoder/NvEncoder.h
    NvEncoder/NvEncoderCuda.h
)

set(SOURCE_FILES
    main.cpp
    NvEncoder/NvEncoder.cpp
    NvEncoder/NvEncoderCuda.cpp
)

add_executable(TestClient ${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(TestClient
    ${OPENCV_LIBRARIES}
    ${MOONVDEC_LIBRARIES}
    ${SATLIB_LIBRARIES}
    ${NVCODEC_LIBRARIES}
)
