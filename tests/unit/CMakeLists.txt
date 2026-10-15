dodoe_add_test(UnitTestMath LABEL unit TIMEOUT 60 SOURCES core/test_math.cpp)

dodoe_add_test(UnitTestCakery LABEL unit TIMEOUT 120
    SOURCES editor/test_console.cpp editor/test_mcp_protocol.cpp
    LIBS EditorCore)
