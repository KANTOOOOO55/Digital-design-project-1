# cmake/run_test.cmake
# Called by CTest to run one simulator test and compare output.

execute_process(
    COMMAND ${SIMULATOR} ${V_FILE} ${STIM_FILE} ${OUT_FILE}
    RESULT_VARIABLE run_result
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Simulator exited with non-zero code: ${run_result}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT_FILE} ${EXP_FILE}
    RESULT_VARIABLE diff_result
)

if(NOT diff_result EQUAL 0)
    message(FATAL_ERROR
        "Output does not match expected.\n"
        "  actual:   ${OUT_FILE}\n"
        "  expected: ${EXP_FILE}"
    )
endif()
