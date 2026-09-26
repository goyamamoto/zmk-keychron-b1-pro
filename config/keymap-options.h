/* SPDX-License-Identifier: MIT
 *
 * Compile-time options of config/keychron_b1_pro.keymap. Edit, then rebuild
 * with `bash scripts/build-firmware.sh build`. ZMK Studio can also change
 * these keys at runtime.
 */
#pragma once

/* 1: the two keys beside Space switch the IME when tapped (left: IME off,
 * right: IME on) and stay modifiers when held. 0: they are plain modifiers.
 * Set 0 if you do not type Japanese: a tap of Alt or Cmd alone would
 * otherwise send an IME key. */
#define B1_IME_TAP 1

/* 1: the Caps Lock key is Left Ctrl and the Left Ctrl key is Caps Lock on the
 * base layers of both OS modes. 0: the keys are as printed. */
#define B1_SWAP_CTRL_CAPS 1
