# The terminal's unit tests, on autobleem-core's harness (doctest, ab_add_test). Host builds only.
include(${CMAKE_SOURCE_DIR}/autobleem-core/tests/harness.cmake)

ab_add_test(test_vt_screen test_vt_screen.cpp LIBS terminal_core)
ab_add_test(test_key_encoder test_key_encoder.cpp LIBS terminal_core)
ab_add_test(test_shell_launch test_shell_launch.cpp LIBS terminal_core)
ab_add_test(test_pty_process test_pty_process.cpp LIBS terminal_core)
