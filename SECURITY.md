# Security Policy

Please report security problems privately: open the **Security** tab of this repository and choose **Report a vulnerability**, rather than opening a public Issue. Japanese or English is fine.

The release files carry a signed provenance attestation; `gh attestation verify <file> -R goyamamoto/zmk-keychron-b1-pro` checks that a download is what GitHub Actions built from the tagged commit. CodeQL, OpenSSF Scorecard and zizmor run on the repository; their findings are in the Security tab.

This is a personal project maintained on a best-effort basis. Problems in ZMK, Zephyr or the bootloader themselves are best reported to their maintainers.
