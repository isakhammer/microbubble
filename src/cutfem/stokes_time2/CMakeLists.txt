add_library(stokes-time2 stokes.cc rhs.cc)
deal_ii_setup_target(stokes-time2)
target_link_libraries(stokes-time2 base flow)

# Executables
add_executable(run-time2 run.cc stokes.cc rhs.cc)
deal_ii_setup_target(run-time2)
target_link_libraries(run-time2 stokes-time2)

add_executable(convergence-time2 convergence.cc)
deal_ii_setup_target(convergence-time2)
target_link_libraries(convergence-time2 stokes-time2)
