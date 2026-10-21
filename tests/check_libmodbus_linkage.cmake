# SPDX-License-Identifier: Apache-2.0
#
# cmake -DBINARY=<file> -DREADELF=<readelf> -DOTOOL=<otool> -DNM=<nm> -P check_libmodbus_linkage.cmake
#
# Fails unless BINARY loads libmodbus as a shared library and contains none of
# its code. On an ELF platform the dynamic section must list libmodbus.so as
# NEEDED (readelf -d); on macOS the load commands must name libmodbus's dylib
# (otool -L). Either way, nm must show modbus_new_rtu as undefined in BINARY,
# not defined: a defined symbol means libmodbus code was linked statically.
foreach(variable BINARY NM)
  if(NOT ${variable})
    message(FATAL_ERROR "${variable} is not set")
  endif()
endforeach()

if(READELF)
  execute_process(COMMAND "${READELF}" -d "${BINARY}"
    OUTPUT_VARIABLE dynamic RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "readelf -d ${BINARY} failed")
  endif()
  if(NOT dynamic MATCHES "\\(NEEDED\\)[^\n]*\\[libmodbus\\.so")
    message(FATAL_ERROR "libmodbus.so is not NEEDED by ${BINARY}:\n${dynamic}")
  endif()
  message(STATUS "readelf -d: libmodbus.so is NEEDED")
elseif(OTOOL)
  execute_process(COMMAND "${OTOOL}" -L "${BINARY}"
    OUTPUT_VARIABLE libraries RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "otool -L ${BINARY} failed")
  endif()
  if(NOT libraries MATCHES "libmodbus[.0-9]*\\.dylib")
    message(FATAL_ERROR "libmodbus is not loaded by ${BINARY}:\n${libraries}")
  endif()
  message(STATUS "otool -L: libmodbus dylib is loaded")
else()
  message(FATAL_ERROR "neither readelf nor otool was found")
endif()

execute_process(COMMAND "${NM}" "${BINARY}" OUTPUT_VARIABLE symbols RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "nm ${BINARY} failed")
endif()
if(symbols MATCHES "[0-9a-fA-F]+ [TtDd] _?modbus_new_rtu\n")
  message(FATAL_ERROR "${BINARY} defines modbus_new_rtu: libmodbus is linked statically")
endif()
if(NOT symbols MATCHES " U _?modbus_new_rtu")
  message(FATAL_ERROR "${BINARY} does not import modbus_new_rtu")
endif()
message(STATUS "nm: modbus_new_rtu is imported, not defined")
