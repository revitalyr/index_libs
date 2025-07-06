target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET CXX_MODULES FILES
        types.cxx
        storage.cxx
)

if (UNIX)
    target_sources(${PROJECT_NAME}
        PUBLIC FILE_SET CXX_MODULES FILES
            bfd_wrapper.cxx
    )
endif (UNIX)