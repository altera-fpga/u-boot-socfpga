.. SPDX-License-Identifier: GPL-2.0+
.. Copyright (C) 2026 Altera Corporation <www.altera.com>

SoC FPGA sandbox tests
======================

The ``socfpga`` U-Boot unit-test suite covers Altera SoC FPGA driver
behaviour under ``CONFIG_SANDBOX``. It is invoked as ``ut socfpga``
and lives in ``test/socfpga/``. Each per-driver test file under that
directory contributes its tests via the ``SOCFPGA_TEST()`` macro
declared in ``include/test/socfpga.h``; no further sandbox-harness
wiring is needed as new drivers get sandbox-ported.

Today the suite ships with one driver - Remote System Update (RSU) -
exercised by ``test/socfpga/rsu.c``. FPGA manager, SDM mailbox,
sysmgr, etc. are expected to add their own ``test/socfpga/<driver>.c``
files using the same pattern.

Suite scope and entry points
----------------------------

The ``ut socfpga`` subcommand is registered by:

* ``include/test/socfpga.h`` - ``SOCFPGA_TEST(name, flags)`` macro
  (mirrors ``CMD_TEST`` in ``include/test/cmd.h``).
* ``test/socfpga/{Kconfig,Makefile}`` - per-driver test gating.
  Default-``y`` on ``CONFIG_SANDBOX`` via ``CONFIG_UT_SOCFPGA``; no
  entries needed in ``configs/sandbox_defconfig``.
* ``test/cmd_ut.c`` - ``SUITE_DECL(socfpga)`` + ``SUITE()`` entry
  that puts ``ut socfpga`` next to ``ut env`` / ``ut bootstd`` /
  ``ut dm``.

Suite tests follow the U-Boot prefix convention
``socfpga_test_<driver>_<thing>``. The ``socfpga_test_`` prefix is
stripped by ``run_suite()``, so output looks like ``Test: rsu_usage:
rsu.c`` and tests can be selected with ``ut socfpga rsu_usage`` (no
``socfpga_`` retyping).

RSU sandbox harness
-------------------

The SoC FPGA Remote System Update (RSU) command stack
(``cmd/socfpga_rsu.c``) is normally driven from real Stratix 10 /
Agilex hardware: the SDM mailbox handles firmware status / update
calls, and a Cadence QSPI flash holds the SPT / CPB metadata. Neither
exists on a Linux host, so the dispatcher, the generic helpers, and
the S10 / Agilex command handlers each get a small amount of sandbox
plumbing in ``drivers/misc/`` that lets ``cmd/socfpga_rsu.c`` link,
run, and be exercised by ``ut socfpga``.

The harness deliberately ``#include``\s the production ``.c`` files
rather than re-implementing their logic (the U-Boot test-harness
rule: "the harness MUST #include the actual production .c, not
re-implement it"). The only thing the sandbox replaces is the
hardware boundary - the SDM mailbox and the QSPI low-level backend.

What the RSU tests exercise
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Production source pulled into the sandbox via thin wrappers in
``drivers/misc/``:

================================================== ==================================================
Production .c                                      Sandbox wrapper
================================================== ==================================================
``arch/arm/mach-socfpga/rsu.c``                    ``drivers/misc/socfpga_rsu_core.c``
``arch/arm/mach-socfpga/rsu_misc.c``               ``drivers/misc/socfpga_rsu_misc.c``
``arch/arm/mach-socfpga/rsu_s10.c``                ``drivers/misc/socfpga_rsu_s10_handlers.c``
================================================== ==================================================

Sandbox-only files (no production peer):

* ``drivers/misc/rsu_ll_sandbox.c`` - a RAM-backed implementation of
  ``rsu_ll_qspi_init()`` (the cross-arch entry point the dispatcher
  calls) returning an empty SPT / CPB. Also hosts the SDM mailbox
  RSU subset stubs ``mbox_rsu_status`` / ``mbox_rsu_get_spt_offset``
  / ``mbox_rsu_update``, each returning ``-EOPNOTSUPP``.

Skipped on purpose:

* ``arch/arm/mach-socfpga/rsu_ll_qspi.c`` - peer of the sandbox LL
  backend behind the same ``struct rsu_ll_intf``; pulling it in
  would require mocking the entire Cadence-QSPI + SPI-flash stack
  and create two competing backends.
* ``arch/arm/mach-socfpga/rsu_spl.c`` - SPL-only multiboot helper;
  the sandbox builds U-Boot proper, so there is no reader.

The "no firmware" contract
~~~~~~~~~~~~~~~~~~~~~~~~~~

Every SDM mailbox call returns ``-EOPNOTSUPP`` in the sandbox. Tests
must therefore assert on the production error paths, not on a happy
case that would require a live SDM. The contract:

* ``rsu_print_status()`` always returns ``-ENOTSUPP`` (hard-coded in
  ``rsu_s10.c``), so any downstream check of the form
  ``if (err == -ENOTSUPP)`` in ``rsu_dtb()`` keeps working.
* The cmd dispatcher in ``cmd/socfpga_rsu.c`` normalises non-zero
  handler returns to ``CMD_RET_FAILURE``, which ``run_command()``
  collapses to ``1``; tests of the form ``run_command("rsu list",
  0)`` therefore assert ``ut_asserteq(1, ...)`` and not
  ``CMD_RET_FAILURE``.
* The dispatcher itself succeeds: ``rsu_init(NULL)`` returns 0 (the
  RAM-backed LL backend is wired), and ``rsu_slot_count()`` returns
  0 (empty SPT).

Building the sandbox
--------------------

The relevant Kconfig symbols, all default ``y`` on sandbox (no
explicit entries needed in ``configs/sandbox_defconfig``):

* ``CONFIG_UT_SOCFPGA`` - enables the ``ut socfpga`` subcommand.
* ``CONFIG_CMD_SOCFPGA_RSU`` - the ``rsu`` console command.
* ``CONFIG_SOCFPGA_RSU_CORE`` - the cross-arch dispatcher + generic
  helpers (``rsu.c`` / ``rsu_misc.c``). Internal symbol auto-y
  whenever ``CMD_SOCFPGA_RSU=y`` on sandbox.
* ``CONFIG_SOCFPGA_RSU_SANDBOX`` - the RAM-backed LL backend +
  mailbox stubs. Internal symbol auto-y whenever
  ``SOCFPGA_RSU_CORE=y``.
* ``CONFIG_SOCFPGA_RSU_S10_HANDLERS`` - the S10 / Agilex command
  handlers (``rsu_s10.c``). Internal symbol auto-y whenever
  ``SOCFPGA_RSU_SANDBOX=y``.
* ``CONFIG_SOCFPGA_RSU_SF_CS`` - sandbox-side definition of the
  SPT / CPB chip-select; never actually used because the mailbox
  stubs trip the no-firmware early-exit first.

Out-of-tree build::

   $ make O=/tmp/sandbox sandbox_defconfig

If your lab is missing host-tool dependencies, disable a few
unrelated Kconfig symbols here before continuing - none are RSU
dependencies::

   $ ./scripts/config --file /tmp/sandbox/.config -d TOOLS_MKEFICAPSULE
   $ ./scripts/config --file /tmp/sandbox/.config -d EFI_CAPSULE_AUTHENTICATE
   $ ./scripts/config --file /tmp/sandbox/.config -d CMD_DHCP6
   $ make O=/tmp/sandbox olddefconfig

``TOOLS_MKEFICAPSULE`` and ``EFI_CAPSULE_AUTHENTICATE`` both want
``<gnutls/gnutls.h>`` (install ``libgnutls28-dev`` /
``gnutls-devel`` to keep them on). ``CMD_DHCP6`` emits
``initializer element is not constant`` on older glibc/gcc combos.

Build the two artifacts the test runner needs - the ELF and the
DTBs (which include ``arch/sandbox/dts/test.dtb``, gated on
``CONFIG_UT_DM`` and loaded by ``./u-boot -T``)::

   $ make O=/tmp/sandbox -j$(nproc) u-boot
   $ make O=/tmp/sandbox dtbs

``dtbs`` is the kbuild target that builds every ``dtb-y`` entry,
including the ``test.dtb`` / ``other.dtb`` that ``CONFIG_UT_DM``
adds to ``arch/sandbox/dts/Makefile``. ``make all`` would build
both the ELF and the DTBs but also drives ``binman``, which
currently fails on labs whose Python is older than 3.7 (the
``importlib.resources`` stdlib module that binman pulls in was
added in 3.7). The two-step build above sidesteps that without
losing anything the tests need.

Running the tests
-----------------

All SoC FPGA tests, one command::

   $ /tmp/sandbox/u-boot -T -c 'ut socfpga'

To run just one driver's tests, use the prefix-stripped sub-name as
a glob::

   $ /tmp/sandbox/u-boot -T -c 'ut socfpga rsu_*'
   $ /tmp/sandbox/u-boot -T -c 'ut socfpga rsu_usage'

The runner strips the ``socfpga_test_`` prefix from each test name
before display and selection, so what you see is ``Test: rsu_usage:
rsu.c`` and what you type is the short name.

RSU test coverage:

================================================ ==========================================================
Test (after prefix strip)                        What it exercises
================================================ ==========================================================
``rsu_usage``                                    bare ``rsu`` -> ``CMD_RET_USAGE`` (banner asserted)
``rsu_bad_slot``                                 ``rsu_parse_slot`` rejection of garbage / overflow
``rsu_bad_size``                                 ``rsu_parse_hex_int`` rejection of ``> INT_MAX``
``rsu_slot_count_zero``                          dispatcher -> empty SPT -> 0 slots
``rsu_init_exits_clean``                         direct ``rsu_init`` / ``rsu_exit`` cycle
``rsu_slot_by_name_empty``                       ``rsu_slot_by_name`` returns ``-ENAME`` on empty SPT
``rsu_list_no_firmware``                         real ``rsu_spt_cpb_list`` -> ``-EOPNOTSUPP`` mbox path
``rsu_update_no_firmware``                       real ``rsu_update`` -> ``-EOPNOTSUPP`` mbox path
``rsu_update_bad_arg``                           ``rsu_update``'s own ``rsu_parse_hex_u64`` rejection
================================================ ==========================================================

The last three tests are gated by
``CONFIG_SOCFPGA_RSU_S10_HANDLERS`` and verify that the production
handler is reached - not the cmd dispatcher's earlier parser - by
asserting on a string produced inside ``rsu_s10.c``
(``"RSU: Firmware or flash content not supporting RSU"`` and
``"RSU: mbox_rsu_update failed"``).

Adding a new RSU test
---------------------

RSU tests live in ``test/socfpga/rsu.c``. Two patterns are in use:

1. **Dispatcher-collapsed + banner assertions** for the cmd path's
   ``CMD_RET_USAGE`` branch::

       ut_asserteq(1, run_command("rsu update", 0));
       ut_assert_skip_to_linen("rsu - ");

   ``run_command()`` collapses ``CMD_RET_USAGE`` *and*
   ``CMD_RET_FAILURE`` to ``1``, so the ``==1`` assert alone cannot
   distinguish them. The ``skip_to_linen("rsu - ")`` follow-up scans
   the captured console for the ``cmd_usage()`` banner, which is
   only emitted on the ``CMD_RET_USAGE`` path. Mark the test with
   ``UTF_CONSOLE`` to enable the console capture.

2. **Direct dispatcher / handler calls** for the underlying contract::

       ut_asserteq(0, rsu_init(NULL));
       ut_asserteq(-ENAME, rsu_slot_by_name("does_not_exist"));
       rsu_exit();

   This avoids the ``run_command`` collapse and lets the test pin
   down specific error codes.

Use ``UTF_CONSOLE`` in the ``SOCFPGA_TEST`` macro and
``ut_assert_nextlinen("prefix")`` when the test depends on a
production printf string (e.g. ``"RSU: mbox_rsu_update failed"``);
that catches regressions where the handler is rewired to a different
backend that prints something else.

Gate any tests that need ``rsu_s10.c`` behind
``CONFIG_SOCFPGA_RSU_S10_HANDLERS``; tests that only need the
dispatcher belong inside the existing ``CONFIG_SOCFPGA_RSU_CORE``
block.

Extending the suite to a new SoC FPGA driver
--------------------------------------------

Adding tests for another SoC FPGA driver (FPGA manager, SDM
mailbox, sysmgr, ...) is three steps:

1. Create ``test/socfpga/<driver>.c`` and register each test with::

       SOCFPGA_TEST(socfpga_test_<driver>_<what>, UTF_CONSOLE);

2. Add one line to ``test/socfpga/Makefile``::

       obj-$(CONFIG_CMD_<DRIVER>) += <driver>.o

3. If the driver requires sandbox-side stubs (because it talks to
   real hardware on ARM), follow the same wrapper pattern used for
   RSU: a ``drivers/misc/socfpga_<driver>_*.c`` wrapper that
   ``#include``\s the production ``arch/arm/mach-socfpga/<driver>.c``,
   plus a sandbox-only backend that supplies the hardware-boundary
   stubs.

No changes to ``cmd_ut.c``, ``test/Makefile``, ``test/Kconfig`` or
``include/test/socfpga.h`` are required: the suite is already
declared and the linker section will absorb new ``SOCFPGA_TEST``
entries automatically.

Future expansion: RSU full-coverage roadmap
-------------------------------------------

This series (Phase 1) pulls 4 of 9 production RSU files into the
sandbox build (``rsu.c``, ``rsu_misc.c``, ``rsu_s10.c``,
``cmd/socfpga_rsu.c``). The goal is to extend that coverage to
every production source in the RSU stack, one production file
per commit, with shared sandbox fakes in their own prerequisite
commits.

**Convention** (mirrors how commits 4 and 5 of this series are
shaped): each follow-up commit either

- introduces *infrastructure* (a sandbox fake driver, layout
  helpers, a Kconfig path) that subsequent file commits depend
  on, or
- pulls in *one* production ``.c`` file via a thin
  ``drivers/misc/socfpga_<file>.c`` wrapper plus the tests it
  unlocks under ``ut socfpga``.

This keeps each commit independently revertible and bisect-
friendly.

============================================  =================  ================================================
Phase                                          Production files   Sandbox infrastructure prerequisite
============================================  =================  ================================================
**1** (this series)                            ``rsu.c``,         ``rsu_ll_sandbox.c`` empty-SPT LL stub +
                                               ``rsu_misc.c``,    ``-EOPNOTSUPP`` mailbox shims
                                               ``rsu_s10.c``,
                                               ``cmd/socfpga_rsu.c``

**2** (interactive surface)                    ``drivers/misc/    Configurable sandbox mailbox fake +
                                               socfpga_rsu.c``    SMC bridge recorder; retires the
                                               (DM probe),        ``-EOPNOTSUPP`` shims from
                                               ``mailbox_s10.c``, ``rsu_ll_sandbox.c``
                                               ``smc_rsu_s10.c``

**3** (QSPI backend)                           ``rsu_ll_qspi.c``  Sandbox SPI flash device + DT fixture +
                                                                  SPT/CPB layout helpers
                                                                  (``LAYOUT_VALID_3SLOTS``,
                                                                  ``LAYOUT_CORRUPT_SPT0_VALID_SPT1``, ...);
                                                                  retires ``rsu_ll_sandbox.c`` entirely

**4** (SPL helpers, spike-gated)               ``rsu_spl.c``      Sandbox SPL build path *or* guard-relax
                                                                  shim; viability confirmed by a 4-hour
                                                                  spike before the phase is committed to
============================================  =================  ================================================

Each phase ships as its own PR. Phase 2 unlocks the
populated-layout test set (``rsu_init`` / ``rsu_slot_*``
happy-path, ``rsu_clear_error_status``,
``rsu_reset_retry_counter``, ``rsu_dcmf_*``,
``rsu_status_log_forward``, ``rsu_running_factory`` and the DM
probe path). Phase 3 unlocks the QSPI backend behaviour
(multi-flash stitched reads, erase round-up, partition
rename/delete, SPT/CPB corruption variants, save/restore
roundtrips). Phase 4 unlocks the SPL boot-time helpers; it is
explicitly optional because upstream U-Boot has thin sandbox-SPL
support.

When Phase 3 lands, ``drivers/misc/rsu_ll_sandbox.c`` is removed
in the same commit that compiles ``rsu_ll_qspi.c``: the
production backend talking to a sandbox SPI flash supersedes the
empty-SPT stub on every dimension.

References
----------

* RSU production source: ``cmd/socfpga_rsu.c``,
  ``arch/arm/mach-socfpga/rsu.c``,
  ``arch/arm/mach-socfpga/rsu_misc.c``,
  ``arch/arm/mach-socfpga/rsu_s10.c``
* RSU sandbox plumbing: ``drivers/misc/socfpga_rsu_core.c``,
  ``drivers/misc/socfpga_rsu_misc.c``,
  ``drivers/misc/socfpga_rsu_s10_handlers.c``,
  ``drivers/misc/rsu_ll_sandbox.c``
* RSU public ABI: ``include/socfpga_rsu.h``,
  ``include/socfpga_rsu_ll.h``, ``include/socfpga_rsu_misc.h``,
  ``include/socfpga_rsu_s10.h``, ``include/socfpga_mailbox_rsu.h``,
  ``include/socfpga_rsu_flash_if.h``
* Suite plumbing: ``include/test/socfpga.h``,
  ``test/socfpga/Kconfig``, ``test/socfpga/Makefile``,
  ``test/cmd_ut.c`` (``SUITE_DECL(socfpga)`` /
  ``SUITE(socfpga, ...)``)
* Tests: ``test/socfpga/rsu.c``
* DT binding (ARM-only anchor):
  ``doc/device-tree-bindings/misc/altr,socfpga-rsu.yaml``
