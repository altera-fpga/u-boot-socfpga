# SoC FPGA RSU — native ASan/UBSan test harness

This directory hosts a host-compiled test harness for the SoC FPGA
Remote System Update (RSU) driver stack. It runs the production logic
under `-fsanitize=address,undefined` with leak detection, on the build
host, in a few seconds — much faster than booting sandbox or hardware.

## Usage

```bash
# Just the harness (fast, no sandbox build needed)
make tests-socfpga-asan         # from the U-Boot top-level
make                            # from this directory; equivalent

# Optional: fold into `make tests` / `./test/run` (host ASan/UBSan only)
RUN_SOCFPGA_RSU_ASAN=1 make tests
```

A successful run ends with:

```
=== Results: <N> run, <N> passed, 0 FAILED ===
```

## What's in here

| File              | Role                                                                 |
| ----------------- | -------------------------------------------------------------------- |
| `Makefile`        | Builds `rsu_asan_test`, links `rsu_asan.o` + `rsu_prod_shim.o`       |
| `rsu_asan.c`      | Harness body: type shims, fake flash/mailbox, tests                  |
| `rsu_prod_shim.c` | Inclusion shim that compiles the production `rsu.c` natively         |
| `include/`        | Host-side U-Boot header shims (`linux/bitops.h`, `asm/types.h`, ...) |
| `rsu_asan_test`   | Compiled binary (gitignored)                                         |

## Migration roadmap

`rsu_prod_shim.c` already `#include`s the production
`arch/arm/mach-socfpga/rsu.c` so the harness exercises the shipped
binary for that source. The remaining production sources still have
clean-room ports in `rsu_asan.c` (marked with section banners). Each
will migrate to the inclusion-shim pattern over time:

| Source                                | Lines (clean-room) | Migration risk                                             |
| ------------------------------------- | ------------------ | ---------------------------------------------------------- |
| `arch/arm/mach-socfpga/rsu_ll_qspi.c` | ~1100              | High — also gated on the qspi_ctx singleton refactor       |
| `arch/arm/mach-socfpga/rsu_s10.c`     | ~40                | Low                                                        |
| `arch/arm/mach-socfpga/rsu_spl.c`     | ~190               | Medium                                                     |
| `cmd/socfpga_rsu.c`                   | ~210               | Low — argv parsing only                                    |
| `drivers/misc/socfpga_rsu.c`          | ~10                | Trivial                                                    |
| `test/cmd/socfpga_rsu.c`              | ~10                | Trivial                                                    |

The end state is a harness whose only `.c` files are stubs + tests; all
RSU logic comes from the shipped source.

## Limitations

- DM (UCLASS_MISC) anchor coverage is via the existing clean-room
  `t_dm_probe()` test only. `CONFIG_SOCFPGA_RSU_DM` is not enabled in
  the host build today; once `drivers/misc/socfpga_rsu.c` migrates to
  the inclusion-shim pattern, the DM path will go through the
  production driver.
- `CONFIG_SPL_ATF=y` is forced (selects the `invoke_smc()` paths in
  `rsu.c`). The non-ATF / `secure_ram_addr()` paths are not covered on
  host; they require SMC machinery that has no native equivalent.
- Hardware paths (real QSPI flash, mailbox, ATF) are stubbed. The
  harness verifies *contract* logic (argv parsing, SPT/CPB checksums,
  errno preservation, NULL/overflow guards, double-init recovery), not
  actual hardware semantics.
