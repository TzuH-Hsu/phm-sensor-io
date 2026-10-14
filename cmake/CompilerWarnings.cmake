# SPDX-License-Identifier: Apache-2.0
#
# phm_sensor_io_set_warnings(<target>)
#
# Applies the project warning set to a target's own sources. The flags are
# PRIVATE, so they never reach a project that links the library. An INTERFACE
# library has no sources of its own: its headers get the warnings through the
# tests that include them.
function(phm_sensor_io_set_warnings target)
  set(warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
  )
  if(PHM_SENSOR_IO_WARNINGS_AS_ERRORS)
    list(APPEND warnings -Werror)
  endif()

  target_compile_options(${target} PRIVATE ${warnings})
endfunction()
