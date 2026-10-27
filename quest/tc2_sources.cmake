# PORT: Crisis Zone Evolution 2 program with the shared System 23 host.
set(TC "${ROOT}/upstream/tc2")
set(CSZ "${ROOT}/upstream/crszone")
file(GLOB LIFTED CONFIGURE_DEPENDS "${CSZ}/gen/crszone_lifted_*.c")
# Instrument only two generated files, with strict source-hash guards.
# Stage B imports its matching archive and does not regenerate/recompile them.
if(NOT TC2_USE_PRECOMPILED_CORE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(WEAPON_LIFTED "${CMAKE_BINARY_DIR}/weapon-lifted")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tools/prepare_weapon_hooks.py"
    --source "${CSZ}/gen" --output "${WEAPON_LIFTED}" RESULT_VARIABLE WEAPON_HOOK_RESULT)
  if(NOT WEAPON_HOOK_RESULT EQUAL 0)
    message(FATAL_ERROR "Version-specific weapon hooks failed validation")
  endif()
  foreach(PART 00 08)
    list(REMOVE_ITEM LIFTED "${CSZ}/gen/crszone_lifted_${PART}.c")
    list(APPEND LIFTED "${WEAPON_LIFTED}/crszone_lifted_${PART}.c")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CSZ}/gen/crszone_lifted_${PART}.c")
  endforeach()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ROOT}/tools/prepare_weapon_hooks.py")
endif()
set_source_files_properties(${LIFTED} PROPERTIES COMPILE_OPTIONS "-O1;-w;-include;${TC}/include/tc2_mips_rt.h")
set_source_files_properties("${TC}/src/tc2_main.c" PROPERTIES COMPILE_DEFINITIONS "main=tc_game_main;SDL_MAIN_HANDLED")
add_compile_definitions(TC2_ADDR_MASK=0x1FFFFFFFu)
include_directories(BEFORE "${CSZ}/include" "${CSZ}/gen")
# ROM-independent sources. The expensive lifted files live here.
# csz_sub.c / csz_io.c are generated from the proprietary ROM and are NOT included.
set(TC2_CORE_SOURCES
  "${ROOT}/quest/crisis_zone_weapons.c"
  "${TC}/src/tc2_cpu.c" "${TC}/src/tc2_mem.c" "${TC}/src/tc2_ctl.c"
  "${TC}/src/tc2_chips.c" "${TC}/src/tc2_screen.c" "${ROOT}/quest/crisis_zone_text.c"
  "${TC}/src/tc2_main.c" "${TC}/src/tc2_input.c" "${TC}/src/tc2_c435.c"
  "${TC}/src/tc2_render.c" "${TC}/src/h8_periph.c" "${TC}/src/tc2_h8.c"
  "${TC}/src/tc2_audio.c" "${TC}/src/tc2_rom.c" "${TC}/src/eng/romzip.c" "${TC}/src/eng/eng_cfg.c"
  "${TC}/src/eng/c352.c" "${CSZ}/src/csz_game.c" ${LIFTED}
  "${ROOT}/quest/tc2_window.c" "${ROOT}/quest/tc2_gl.c"
  "${ROOT}/quest/quest_host.c" "${ROOT}/quest/quest_gl.c" "${ROOT}/quest/tc2_scene.c"
  "${ROOT}/quest/quest_gun.c" "${ROOT}/quest/quest_options.c" "${ROOT}/quest/quest_cover.c" "${ROOT}/quest/quest_ui.c")
# ROM-generated H8 programs. Linked only into the production libmain.so.
set(TC2_H8_SOURCES
  "${CSZ}/gen/csz_sub.c" "${CSZ}/gen/csz_io.c")
set(TC2_SOURCES ${TC2_CORE_SOURCES} ${TC2_H8_SOURCES})
