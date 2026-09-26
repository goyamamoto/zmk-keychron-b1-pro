# SPDX-License-Identifier: MIT

zephyr_include_directories(include)

# First source of the module: its listener must come before every other
# position listener (src/early_events.c).
if(CONFIG_KB1_EARLY_EVENTS)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/early_events.c)
endif()

if(CONFIG_KB1_CONN_SWITCH)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_conn_switch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/conn_policy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kb1_power.c
  )
endif()

if(CONFIG_KB1_SWITCH_WAKER)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/switch_waker.c)
endif()

if(CONFIG_KB1_KEY_WAKER)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/key_waker.c)
endif()

if(CONFIG_KB1_KSCAN_GPIO_MATRIX_NODIODE)
  # kscan_gpio.h of ZMK's kscan drivers (kscan_gpio_pin_get and friends).
  target_include_directories(app PRIVATE ${APPLICATION_SOURCE_DIR}/module/drivers/kscan)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kscan_gpio_matrix_nodiode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ghost_filter.c
  )
endif()

if(CONFIG_KB1_LEDS)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kb1_leds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/led_logic.c
  )
endif()

if(CONFIG_KB1_INDICATOR_BEHAVIOR)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_kb1_indicator.c)
endif()

if(CONFIG_ZMK_USJIS)
  set(ZMK_USJIS_LISTENER ${CMAKE_CURRENT_SOURCE_DIR}/src/usjis.c)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/usjis_resolver.c
    ${ZMK_USJIS_LISTENER}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_usjis.c
  )

  # Listener order is link order (the .event_subscription section). Modules
  # are processed before ZMK adds its own sources, so usjis.c would come first
  # and run before hold-tap and key repeat. After ZMK's app directory has been
  # processed, move usjis.c to just before hid_listener.c
  # (docs/usjis-architecture.md section 6). The startup check in usjis.c
  # verifies the result.
  function(zmk_usjis_place_listener)
    get_property(listener GLOBAL PROPERTY ZMK_USJIS_LISTENER_SOURCE)
    get_target_property(sources app SOURCES)
    list(REMOVE_ITEM sources ${listener})
    set(index -1)
    set(i 0)
    foreach(source ${sources})
      if(source MATCHES "/hid_listener\\.c$")
        set(index ${i})
      endif()
      math(EXPR i "${i} + 1")
    endforeach()
    if(index EQUAL -1)
      message(FATAL_ERROR "usjis: hid_listener.c not found among the app sources")
    endif()
    list(INSERT sources ${index} ${listener})
    set_property(TARGET app PROPERTY SOURCES "${sources}")
    message(STATUS "usjis: listener placed before hid_listener.c (source ${index})")
  endfunction()
  set_property(GLOBAL PROPERTY ZMK_USJIS_LISTENER_SOURCE ${ZMK_USJIS_LISTENER})
  cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL zmk_usjis_place_listener)
endif()
