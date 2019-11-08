set(OCVTASK_ROOT "${CMAKE_SOURCE_DIR}/OpenCVTask")

include_directories(${OCVTASK_ROOT})

set(HEADER_FILES
    NvDecoder/NvDecoder.h
)

set(SOURCE_FILES
    main.cpp
    NvDecoder/NvDecoder.cpp
)

set(STARBURST_CONF_FILES ${STARBURST_CONF_FILES} "${OCVTASK_ROOT}/starburst" PARENT_SCOPE)

add_executable(OpenCVTask ${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(OpenCVTask
    ${OPENCV_LIBRARIES}
    ${MOONVDEC_LIBRARIES}
    ${SATLIB_LIBRARIES}
    ${NVCODEC_LIBRARIES}
)
