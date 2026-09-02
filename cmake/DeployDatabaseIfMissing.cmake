if(NOT DEFINED SOURCE_DB OR NOT DEFINED DEST_DB)
    message(FATAL_ERROR "SOURCE_DB and DEST_DB are required")
endif()

get_filename_component(DEST_DIR "${DEST_DB}" DIRECTORY)
file(MAKE_DIRECTORY "${DEST_DIR}")

# diet.db contains user profiles, fridge inventory, favorites and personal recipes.
# Rebuilding the executable must never replace an existing runtime database.
if(NOT EXISTS "${DEST_DB}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCE_DB}" "${DEST_DB}"
        RESULT_VARIABLE COPY_RESULT)
    if(NOT COPY_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to deploy initial diet.db")
    endif()
    message(STATUS "Deployed initial diet.db")
else()
    message(STATUS "Preserving existing runtime diet.db")
endif()
