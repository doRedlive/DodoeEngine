if(NOT DODOE_SHIPPING AND TARGET DodoeSandbox)
    add_test(NAME SmokeSandboxLaunch
        COMMAND DodoeSandbox --smoke-test=60
    )
    set_tests_properties(SmokeSandboxLaunch PROPERTIES
        LABELS smoke
        TIMEOUT 60
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
