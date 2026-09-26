# SPDX-License-Identifier: MIT

# Suppresses duplicate unit-address warnings for power, clock, acl and
# flash-controller (as the upstream ZMK nRF52840 boards do).
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
