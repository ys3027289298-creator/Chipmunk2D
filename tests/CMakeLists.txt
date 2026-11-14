# Verification entry points for the solver pipeline refactor.
# These are headless CLI programs; the graphical demos remain in demo/.

add_executable(cp_solver_verify cp_solver_verify.c)
target_include_directories(cp_solver_verify PRIVATE
	${chipmunk_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cp_solver_verify chipmunk_static m)

add_executable(cp_solver_modes cp_solver_modes.c)
target_include_directories(cp_solver_modes PRIVATE
	${chipmunk_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cp_solver_modes chipmunk_static m)

add_executable(cp_solver_diagnostics cp_solver_diagnostics.c)
target_include_directories(cp_solver_diagnostics PRIVATE
	${chipmunk_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cp_solver_diagnostics chipmunk_static m)

# Frame generator for the acceptance screen recording (headless PPM stream).
add_executable(cp_solver_record cp_solver_record.c)
target_include_directories(cp_solver_record PRIVATE
	${chipmunk_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cp_solver_record chipmunk_static m)

# Register with CTest: "ctest --test-dir build" runs the full acceptance suite.
add_test(NAME solver_verify COMMAND cp_solver_verify)
add_test(NAME solver_modes COMMAND cp_solver_modes)
add_test(NAME solver_diagnostics COMMAND cp_solver_diagnostics)
