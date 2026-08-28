if(NOT DEFINED DLL OR NOT DEFINED SOURCE_DIR OR NOT DEFINED RELEASE_DIR OR
   NOT DEFINED PROJECT_VERSION)
  message(FATAL_ERROR "The release packaging arguments are incomplete")
endif()

set(staging "${RELEASE_DIR}/staging")
file(REMOVE_RECURSE "${staging}")
file(MAKE_DIRECTORY "${staging}/SKSE/Plugins")
file(COPY "${DLL}" DESTINATION "${staging}/SKSE/Plugins")
file(COPY "${SOURCE_DIR}/package/SKSE/Plugins/ArcaneActivation.ini"
     DESTINATION "${staging}/SKSE/Plugins")
file(COPY "${SOURCE_DIR}/package/ArcaneActivation.esp"
     DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/package/meshes" DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/package/Scripts" DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/package/Source" DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/package/MCM" DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/docs/ArcaneActivation.md"
     DESTINATION "${staging}")
file(RENAME "${staging}/ArcaneActivation.md" "${staging}/README.md")
file(COPY "${SOURCE_DIR}/LICENSE" DESTINATION "${staging}")
file(COPY "${SOURCE_DIR}/THIRD_PARTY_NOTICES.md" DESTINATION "${staging}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf
          "${RELEASE_DIR}/ArcaneActivation-${PROJECT_VERSION}.zip"
          --format=zip -- ArcaneActivation.esp README.md LICENSE
          THIRD_PARTY_NOTICES.md SKSE Scripts Source MCM meshes
  WORKING_DIRECTORY "${staging}"
  COMMAND_ERROR_IS_FATAL ANY
)
