cmake_minimum_required(VERSION 3.16)
project(EventDrivenSimulatorQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(EventDrivenSimulatorQt main.cpp)

target_compile_definitions(EventDrivenSimulatorQt PRIVATE PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set(SAMPLE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/example.v
    ${CMAKE_CURRENT_SOURCE_DIR}/example.stim
)

foreach(sample_file IN LISTS SAMPLE_FILES)
    add_custom_command(TARGET EventDrivenSimulatorQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${sample_file}
                $<TARGET_FILE_DIR:EventDrivenSimulatorQt>
        VERBATIM)
endforeach()

if(APPLE)
    set_target_properties(EventDrivenSimulatorQt PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "EventDrivenSimulatorQt"
    )

    foreach(sample_file IN LISTS SAMPLE_FILES)
        get_filename_component(sample_name ${sample_file} NAME)
        set_source_files_properties(${sample_file} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)

        add_custom_command(TARGET EventDrivenSimulatorQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${sample_file}
                    $<TARGET_BUNDLE_CONTENT_DIR:EventDrivenSimulatorQt>/Resources/${sample_name}
            VERBATIM)
    endforeach()
endif()
