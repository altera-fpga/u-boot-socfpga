.. SPDX-License-Identifier: GPL-2.0+
.. sectionauthor:: Tien Fong Chee <tien.fong.chee@altera.com>

Agilex 5 SoCDK
==============

This page covers the U-Boot defconfigs available for the Altera Agilex 5
SoC FPGA family, and which one to pick for production firmware versus
board bring-up / Simics emulation.

Root of Trust on Agilex 5
-------------------------

Two independent authentication paths are supported on Agilex 5; a build
is considered to have a Root of Trust when **at least one** is enabled:

* ``CONFIG_SPL_FIT_SIGNATURE`` - upstream U-Boot RSA-signed FIT
  verification performed inside SPL using a public key embedded in the
  SPL device tree.
* ``CONFIG_SOCFPGA_SECURE_VAB_AUTH`` - Altera Verified Authentication
  via SDM, which post-processes the FIT image through the Secure
  Device Manager with SHA-384 (see ``arch/arm/mach-socfpga/Kconfig``).

A build with neither symbol enabled has no Root of Trust and will
boot any image written to flash. ``arch/arm/mach-socfpga/Makefile``
emits a ``$(warning ...)`` whenever an Agilex 5 build is configured
this way so that the line appears in every CI log and developer build
output.

Defconfig variant matrix
------------------------

Status as of ``socfpga_v2026.04_RC``. Effective values reflect what
the C-preprocessor pass U-Boot runs over defconfigs
(``scripts/kconfig/Makefile``) produces after expanding
``#include <configs/...>`` directives:

.. list-table::
   :header-rows: 1
   :widths: 35 22 12 31

   * - Defconfig
     - SPL FIT signature
     - VAB
     - Intended use
   * - ``socfpga_agilex5_defconfig``
     - ``=y``
     - unset
     - **Production** reference. Use this as the starting point for
       any shipping firmware.
   * - ``socfpga_agilex5_oobe2_defconfig``
     - ``=y``
     - unset
     - Customer Out-Of-Box Experience template. Safe as a starting
       point for a customer product defconfig.
   * - ``socfpga_agilex5_vab_defconfig``
     - ``=y`` (inherited)
     - ``=y``
     - VAB-authenticated build. SDM verifies the FIT via SHA-384;
       FIT signature support is also inherited from the base.
   * - ``socfpga_agilex5_modular_defconfig``
     - ``=y`` (inherited)
     - unset
     - Modular DevKit reference; inherits FIT signature verification
       from the base defconfig via ``#include``.
   * - ``socfpga_agilex5_a0_defconfig``
     - ``=y`` (inherited)
     - unset
     - A0 silicon variant; inherits from the base defconfig.
   * - ``socfpga_agilex5_a0_modular_defconfig``
     - ``=y`` (inherited)
     - unset
     - A0 silicon Modular DevKit variant; inherits from base via the
       modular defconfig.
   * - ``socfpga_agilex5_a0_emmc_defconfig``
     - ``=y`` (inherited)
     - unset
     - A0 silicon eMMC boot variant; inherits from base via the eMMC
       defconfig.
   * - ``socfpga_agilex5_013b_defconfig``
     - ``=y`` (inherited)
     - unset
     - ESeries 013B DevKit; inherits from the base defconfig.
   * - ``socfpga_agilex5_nand2_defconfig``
     - ``=y`` (inherited)
     - unset
     - NAND boot variant; inherits from the base defconfig.
   * - ``socfpga_agilex5_emmc_defconfig``
     - ``=y`` (inherited)
     - unset
     - eMMC boot variant; inherits from the base defconfig.
   * - ``socfpga_agilex5_de25_nano_defconfig``
     - ``=y`` (inherited)
     - unset
     - Terasic DE25 Nano board; inherits from the base defconfig.
   * - ``socfpga_agilex5_debug2_defconfig``
     - ``=n``
     - unset
     - **Not for shipping firmware.** Bring-up / engineering defconfig
       that permits unsigned legacy uImage so developers can iterate
       without signing every build.
   * - ``socfpga_agilex5_emu_defconfig``
     - ``=n``
     - unset
     - **Not for shipping firmware.** Simics emulation; emulation has
       no Root of Trust hardware path.

Only the two standalone defconfigs ``socfpga_agilex5_debug2_defconfig``
and ``socfpga_agilex5_emu_defconfig`` explicitly set
``CONFIG_SPL_FIT_SIGNATURE=n``; they are also the only Agilex 5
defconfigs that trip the ``arch/arm/mach-socfpga/Makefile`` warning
about a missing Root of Trust. Every other variant either signs the
FIT (inherited from ``socfpga_agilex5_defconfig`` via ``#include``)
or authenticates through VAB (``socfpga_agilex5_vab_defconfig``).

Picking a defconfig for a shipping product
------------------------------------------

For RSA-signed FIT verification (upstream U-Boot Root of Trust):
start from ``socfpga_agilex5_defconfig`` or
``socfpga_agilex5_oobe2_defconfig``. The board-specific variants
that ``#include`` the base (``modular``, ``a0*``, ``013b``,
``nand2``, ``emmc``, ``de25_nano``) inherit the FIT signature check
automatically; keep the ``#include`` line when deriving a product
defconfig from them.

For SDM-backed VAB authentication (Altera-specific Root of Trust):
start from ``socfpga_agilex5_vab_defconfig``, which also inherits the
FIT signature path from the base.

Do **not** ship a derivative of ``socfpga_agilex5_debug2_defconfig``
or ``socfpga_agilex5_emu_defconfig`` without enabling either VAB or
FIT signing first - those are the only two Agilex 5 defconfigs that
explicitly disable the FIT signature check.

Signing the FIT (SPL_FIT_SIGNATURE path)
----------------------------------------

Enabling ``CONFIG_SPL_FIT_SIGNATURE=y`` turns on signature
*verification* in SPL but does not by itself produce a signed image.
The signing flow is:

1. Generate an RSA key pair (or reuse the production key).
2. Add a ``signature`` node referencing the public key to the SPL
   device tree.
3. Sign the FIT with ``mkimage -K <spl-dtb> -k <keydir> -F <fit>``.
4. Verify by booting on real silicon - SPL will reject an unsigned or
   mis-signed FIT with ``Failed to verify required signature``.

See ``doc/usage/fit/signature.rst`` for the upstream U-Boot signature
flow and ``tools/mkimage`` for the signing tool.
