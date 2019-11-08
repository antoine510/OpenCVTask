set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/OCVTask-$<CONFIG>")

message(STATUS "Using ${OUTPUT_DIR} as output folder")

set(RUN_CMAKE_COMMAND ${CMAKE_COMMAND} "-E")

add_custom_target("__make_output_dir"
    COMMAND ${RUN_CMAKE_COMMAND} make_directory ${OUTPUT_DIR}
    COMMENT "Making output directory ${OUTPUT_DIR}"
)
set(DEPLOY_DEPS "__make_output_dir")

if(STARBURST_CONF_FILES)
    add_custom_target("__copy_starburst"
        DEPENDS "__make_output_dir"
        COMMAND ${RUN_CMAKE_COMMAND} copy_directory ${STARBURST_CONF_FILES} "${OUTPUT_DIR}/starburst"
        COMMENT "Copying Starburst files to ${OUTPUT_DIR}"
    )
    list(APPEND DEPLOY_DEPS "__copy_starburst")
else()
    message(WARNING "No Starburst configuration files detected")
endif()


set(COPIED_BINARIES ${OPENCV_BINARIES} ${MOONVDEC_BINARIES} ${SATLIB_BINARIES})
add_custom_target("__copy_binaries"
    DEPENDS "__make_output_dir"
    COMMAND ${RUN_CMAKE_COMMAND} copy_if_different ${COPIED_BINARIES} ${OUTPUT_DIR}
    COMMENT "Copying binary files to ${OUTPUT_DIR}"
)
list(APPEND DEPLOY_DEPS "__copy_binaries")

set(ACTIVE_TARGETS $<TARGET_FILE:OpenCVTask>)
if(BUILD_TEST_CLIENT)
    list(APPEND ACTIVE_TARGETS $<TARGET_FILE:TestClient>)
endif()

add_custom_target("__copy_targets"
    DEPENDS "__make_output_dir"
    COMMAND ${RUN_CMAKE_COMMAND} copy_if_different ${ACTIVE_TARGETS} ${OUTPUT_DIR}
    COMMENT "Copying active targets to ${OUTPUT_DIR}"
)
list(APPEND DEPLOY_DEPS "__copy_targets")

add_custom_target(deploy
    DEPENDS ${DEPLOY_DEPS}
)
