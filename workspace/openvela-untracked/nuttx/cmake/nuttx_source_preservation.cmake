# SPDX-License-Identifier: Apache-2.0

# Opt in when a Make-configured source tree must remain available.  This does
# not reuse its .config or generated links: all CMake outputs remain separate.
option(NUTTX_PRESERVE_SOURCE "Keep a Make-configured source tree intact" OFF)

# Check the destination and every parent before writing a generated file.  A
# lexical binary-tree path alone is insufficient when an old output is a link.
function(nuttx_require_binary_path path)
  if(NOT NUTTX_PRESERVE_SOURCE)
    return()
  endif()
  get_filename_component(destination "${path}" ABSOLUTE)
  file(RELATIVE_PATH relative "${CMAKE_BINARY_DIR}" "${destination}")
  if(relative MATCHES "^\\.\\.(/|$)" OR IS_ABSOLUTE "${relative}")
    message(FATAL_ERROR "Generated path is outside the build tree: ${path}")
  endif()
  while(TRUE)
    if(IS_SYMLINK "${destination}")
      message(FATAL_ERROR "Generated path must not use a symlink: ${destination}")
    endif()
    if(destination STREQUAL CMAKE_BINARY_DIR)
      break()
    endif()
    get_filename_component(destination "${destination}" DIRECTORY)
  endwhile()
endfunction()

if(NUTTX_PRESERVE_SOURCE)
  get_filename_component(source_real "${NUTTX_DIR}" REALPATH)
  get_filename_component(binary_real "${CMAKE_BINARY_DIR}" REALPATH)
  file(RELATIVE_PATH binary_relative "${source_real}" "${binary_real}")
  file(RELATIVE_PATH source_relative "${binary_real}" "${source_real}")
  if(NOT binary_relative MATCHES "^\\.\\./"
     OR NOT source_relative MATCHES "^\\.\\./")
    message(FATAL_ERROR
      "NUTTX_PRESERVE_SOURCE requires separate, non-nested source/build trees")
  endif()

  # These entries are written during configuration/context generation.  Reject
  # old output links before the first configure_file(), Kconfig call or write.
  foreach(output
      .config .config.compressed .config.orig .config.prev
      .defconfig.processed defconfig defconfig.tmp defconfig.orig
      config_tree.json config_tree_dirty.json CMakeCache.txt
      include/nuttx/config.h include/nuttx/version.h include/arch
      include/arch/chip drivers/platform/Kconfig
      boards/dummy/Kconfig arch/dummy/Kconfig)
    nuttx_require_binary_path("${CMAKE_BINARY_DIR}/${output}")
  endforeach()
elseif(EXISTS "${NUTTX_DIR}/.config")
  message(FATAL_ERROR
    "Please distclean previous make build with `make distclean`")
endif()
