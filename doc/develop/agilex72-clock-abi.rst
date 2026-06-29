.. SPDX-License-Identifier: GPL-2.0+

Agilex 72 clock dt-binding ABI policy
=============================================

The Altera Agilex 72 family has its
clock manager described by the public dt-binding header
``include/dt-bindings/clock/altr,agilex72-clock.h``. The Linux
SoCFPGA tree carries a sibling header in
``altera-innersource/applications.fpga.soc.linux-socfpga-dev``
(``include/dt-bindings/clock/altr,agilex72-clkmgr.h``).

Namespace detach (May 2026)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Earlier U-Boot bring-up revisions aliased every
``AGILEX72_*`` symbol to a matching ``AGILEX5_*`` integer so the
Agilex 72 driver could be brought up on top of the existing Agilex5
clock-driver infrastructure. That alias chain has been removed in
favour of a self-contained integer namespace that **matches the
Linux header exactly for IDs 0..85**. U-Boot extensions (CORE2/CORE3 for
pre-V9 A76 silicon, the WDT_CLK alias) live above 85. A DTB compiled
against either header therefore carries identical clock-reference
integers, which is the precondition for the longer-term goal of
having U-Boot consume the upstream Linux DTS verbatim.

The two headers still track **different revisions** of the Agilex 72 HPS
Clock Manager HAS document and therefore use **different symbolic
names** for a handful of clocks. The underlying integer IDs match,
so the DTB stays ABI-compatible between U-Boot and Linux.

This file is the canonical record of that split, the rationale, and
the open coordination items between the U-Boot SoCFPGA maintainers
and Linux SoCFPGA maintainers.

Versioning policy
-----------------

* **Linux** tracks **HAS v0.8** (the revision the Linux binding was
  authored against).
* **U-Boot** tracks **HAS v0.83** (the revision in effect at the
  time the handoff-parser and dt-binding work landed; Rev 0.83 was published
  on 13 February 2026 and is the canonical reference for new
  Agilex 72 consumers).

Both revisions describe the same silicon. Where v0.83 renames or
splits something, U-Boot uses the newer name; Linux keeps the
older name; the underlying integer ID is **identical**, so a DTB
produced by either project resolves correctly on the other.

Mapping table
-------------

The four columns are:

* **Integer ID**: the value the DTC emits into the DTB and the
  driver switches on.
* **U-Boot (v0.83)**: the symbolic name in the U-Boot header.
* **Linux (v0.8)**: the symbolic name in the Linux header.
* **Notes**: rationale + cross-reference to the HAS table that
  documents the rename.

============  ===========================================  ==========================================  =================================
Integer ID    U-Boot (v0.83 spelling)                      Linux (v0.8 spelling)                       Notes
============  ===========================================  ==========================================  =================================
7             ``AGILEX72_GPPLL1_C0_CLK`` (+ optional    ``AGILEX72_GPPLL1_C0_CLK``              GPPLL1 has one PLL output.
               ``..._C1_CLK`` alias)                                                                 HAS v0.83 Table 171 (Appendix C
                                                                                                       GPPLL division) still assigns it
                                                                                                       to **C0** / Fclkout0, not C1. Linux
                                                                                                       Linux uses ``GPPLL1_C0_CLK`` at ID
                                                                                                       7. U-Boot defines ``GPPLL1_C1_CLK``
                                                                                                       as a preprocessor alias only (U-Boot
                                                                                                       driver / ``KM_HPS_Clocks_Spreadsheet``
                                                                                                       GPPLL Config column labels); not a
                                                                                                       second clock and not a HAS rename.
25            ``AGILEX72_XSPIPHY_FREE_CLK``            ``AGILEX72_XSPI_PHY_FREE_CLK``          Spelling alias.
26            ``AGILEX72_MEMPHY_CLK``                  ``AGILEX72_MEMDEVICE_PHY_FREE_CLK``     Spelling alias.
73            ``AGILEX72_USB31_REF_CLK``               ``AGILEX72_USB31_SUSPEND_CLK``          v0.83 documents a SINGLE 20 MHz
                                                                                                       Clock-Manager output that feeds
                                                                                                       BOTH the USB31 ``ref_clk`` and
                                                                                                       ``suspend_clk`` pins (HAS-GPPLL
                                                                                                       Tables 16, 49, 59 + Sec 9.8.5:
                                                                                                       "single PING PONG COUNTER and
                                                                                                       clock gate"). U-Boot's
                                                                                                       ``USB31_REF_CLK`` is an alias
                                                                                                       of ``USB31_SUSPEND_CLK`` so the
                                                                                                       dwc3 named-clocks binding
                                                                                                       works ("ref" + "suspend").
79            ``AGILEX72_XSPIPHY_CLK``                 ``AGILEX72_XSPI_PHY_CLK``               Spelling alias.
82            ``AGILEX72_SDMMC0_CLK``                  ``AGILEX72_SDMMC0_SDMCLK``              Spelling alias (SDMMC0).
83            ``AGILEX72_SDMMC1_CLK``                  ``AGILEX72_SDMMC1_SDMCLK``              Spelling alias (SDMMC1).
38            ``AGILEX72_WDT_CLK``                     *no equivalent*                             U-Boot defines WDT_CLK as an
                                                                                                       alias of LSP_SYS_FREE_CLK (HAS
                                                                                                       v0.83 Section 9.8.9). Linux
                                                                                                       binds the WDT pclk to
                                                                                                       LSP_SYS_FREE_CLK directly.
86..89        ``AGILEX72_CORE{2,3}{_FREE,}_CLK``       *no equivalent*                             U-Boot-only extensions for
                                                                                                       V9 A720 cores 2/3 (GPPLL2).
                                                                                                       Linux has not
                                                                                                       modelled these IDs; the
                                                                                                       integer range does not
                                                                                                       collide with Linux 0..85.
============  ===========================================  ==========================================  =================================

All other ``AGILEX72_*`` names in the U-Boot header use the Linux
header's symbolic names and integer IDs verbatim. The two headers
diverge only on the rows above.

Audit outcomes (F-items)
------------------------

The dt-binding audit against the JY documentation bundle produced a
short list of "F" findings (F1-F5). Their current resolution
status:

F1 (USB2 OTG clock domain)
   The Agilex 72 HPS exposes a separate USB2OTG IP (dwc2, at
   ``usb@0d100000``) alongside USB3.1 (dwc3, at ``usb31@d000000``);
   they live in different clock domains per HAS v0.83. Table 46
   (USB2OTG clock domains) routes the AHB / OCP ``hclk`` and
   ``utmi_clk`` of USB2OTG from ``hsp_mp_clk``. On the v0.83
   default preset that lane is 500 MHz (GPPLL0_C0 / 2 per HAS
   Table 54; see F10). Table 47 (USB31 clock domains)
   lists ``hsp_main_clk`` (1000 MHz) as the USB31 wrapper's
   ``bus_clk_early`` and a single pin-muxed
   ``usb31_suspend_and_ref_clk`` output (20 MHz, GPPLL0_C0/50)
   that feeds BOTH the USB31 ``ref_clk`` and ``suspend_clk``
   pins. HAS-GPPLL Tables 16 / 49 / 59 list one Clock-Manager
   output and Section 9.8.5 ("single PING PONG COUNTER and
   clock gate") makes the shared-wire intent explicit, so the
   "(20 / 25 MHz)" reading of older HAS revisions is a doc
   artefact, not two distinct clocks.

   ``socfpga_km.dtsi`` reflects this:

   * ``usb0`` (dwc2) binds ``AGILEX72_HSP_MP_CLK`` with
     ``clock-names = "otg"``. Matches the Linux dwc2 binding.

   * ``usb31`` (dwc3) binds three CM-sourced clocks:
     ``AGILEX72_USB31_REF_CLK``, ``AGILEX72_HSP_MAIN_CLK``,
     ``AGILEX72_USB31_SUSPEND_CLK`` with ``clock-names = "ref",
     "bus_early", "suspend"``. The Linux DTS leaves ``usb31``
     commented out and bound to only two clocks (``USB31_SUSPEND``
     + ``HSP_MAIN``); the U-Boot binding is strictly more
     HAS-complete and exposes all three named inputs the dwc3 IP
     declares.

   * ``usbphy0`` (``usb-nop-xceiv``) has no ``clocks`` property.
     The off-die USB2 PHY's only clock (``ulpi_clk`` per Table 46)
     comes from IO Pinmux, not the Clock Manager. Matches Linux.

   ``AGILEX72_HSP_MAIN_CLK`` (integer ID 34) is the gated
   sibling of ``AGILEX72_HSP_MAIN_FREE_CLK``; both run at
   1000 MHz on the v0.83 default preset.

   An earlier driver revision mis-mapped all three USB nodes to
   ``AGILEX72_USB31_REF_CLK`` because it treated a legacy generic
   USB clock alias as interchangeable across domains; that
   effectively gave ``usb0`` and ``usbphy0`` a 20 MHz reference
   instead of the ``hsp_mp_clk`` lane they actually consume. The
   current binding follows HAS Tables 46 + 47 and the corrected
   ``HSP_MP`` rate in F10.

F6 (Watchdog clock source)
   HAS v0.83 Section 9.8.9 ("Watch Dog Timer Clock") was added on
   10 November 2025 with the changelog note "added for
   clarification". The section body explicitly defers all
   frequency information to ``KM_HPS_Clocks_Spreadsheet.xlsx``,
   which lists the WDT pclk as 125 MHz sourced from
   ``lsp_sys_free_clk`` with a 1:1 ratio. There is no separate
   ``wdt_clk`` Clock-Manager output; Section 9.8.9 documents the
   existing wire, not a new one.

   ``socfpga_km.dtsi`` therefore binds all four watchdogs to
   ``AGILEX72_LSP_SYS_FREE_CLK``, matching Linux. The
   binding header keeps ``AGILEX72_WDT_CLK`` as an alias of
   ``AGILEX72_LSP_SYS_FREE_CLK`` (same integer ID) for any
   consumer that prefers the descriptive name; an earlier driver
   revision allocated WDT_CLK its own integer ID, which was
   incorrect and has been retired.

F7 (EMAC PTP reference clock)
   The legacy Agilex 72 dtsi (pre-v0.83 binding) declared only the
   ``stmmaceth`` controller clock on ``gmac0/1/2`` and omitted the
   ``ptp_ref`` timestamp reference. HAS image37 (EMAC/XGMAC clock
   generation) shows ``emac_ptp_ref_clk`` as a distinct
   Clock-Manager output. Linux binds it for every EMAC;
   ``socfpga_km.dtsi`` now does the same:

   .. code-block:: dts

       clocks      = <&clkmgr AGILEX72_EMAC[0-2]_CLK>,
                     <&clkmgr AGILEX72_EMAC_PTP_CLK>;
       clock-names = "stmmaceth", "ptp_ref";

   The U-Boot dwc_eth_xgmac_socfpga driver only looks up
   ``"stmmaceth"`` by name today, so adding ``"ptp_ref"`` is a
   no-op at probe time; it matches the Linux binding and enables
   PTP / IEEE-1588 timestamping for any future driver bump.

F8 (SDMMC ``sdmclk`` naming)
   HAS v0.83 Table 18 (SDMMC clock domains) names the SD-card
   ("ciu") clock ``sdmclk`` -- sourced from ``memdevice_phy_clk``,
   distinct from the ``lsp_mp_clk`` that drives the bus interface
   ("biu") side. Linux uses the HAS spelling
   ``AGILEX72_SDMMC{0,1}_SDMCLK`` on ``mmc0`` / ``mmc1``; the
   U-Boot binding header adds the same spellings as aliases of
   the existing ``AGILEX72_SDMMC{0,1}_CLK`` integer IDs.
   ``socfpga_km.dtsi`` references the ``_SDMCLK`` form on both
   MMCs to match Linux. Integer IDs are unchanged.

F2 (GPPLL1 single-output naming)
   HAS v0.83 Table 171 (GPPLL division parameters) places GPPLL1's
   sole enabled C-divider in the **C0** / Fclkout0 column, same as
   the v0.8 snapshot. Linux and the canonical U-Boot symbol
   are ``AGILEX72_GPPLL1_C0_CLK`` (ID 7). ``GPPLL1_C1_CLK`` is
   a preprocessor alias only; it does not reflect a HAS v0.83 rename
   from C0 to C1. The ``_C1_`` suffix in early driver code
   (``gppll1_c1_div``) follows ``KM_HPS_Clocks_Spreadsheet`` column
   headers, not the HAS-GPPLL table. Integer ID 7 is shared; DTB is
   interoperable. **No action requested of Linux.**

F3 (no longer tracked)
   Resolved during the v0.83 alignment pass; see commit history
   for the Agilex 72 dt-binding header expansion.

F4 (shared Linux integer namespace)
   Resolved. U-Boot now adopts the Linux integer namespace
   verbatim for IDs 0..85. The two headers continue to use a few
   different symbolic names per the v0.8/v0.83 split, but every
   shared name has the same integer ID. U-Boot-only extensions
   live above 85 and do not collide with the Linux range.

F5 (SDMMC0/SDMMC1 split)
   Resolved. Linux already documents both ``SDMMC0`` and
   ``SDMMC1`` (sdmclk, phy_clk, sdphy_reg_clk variants at IDs
   80..85). U-Boot's header now uses the same integer IDs and
   adds the v0.83 ``..._CLK`` spellings as aliases of the
   ``..._SDMCLK`` form so DTS authors can pick either name.

F9 (xSPI controller clocking)
   HAS v0.83 "xSPI" block (Agilex 72 HPS clocking spreadsheet rows
   277-278) lists the HPS XSPI controller with three IP-side
   clock inputs:

   .. code-block:: text

       regPCLK   125 MHz  lsp_sp_clk  1:1
       mACLK     250 MHz  lsp_mp_clk  1:1
       xspi_clk  250 MHz  lsp_mp_clk  1:1

   HAS-GPPLL Table 20 ("xSPI 0 PHY clock domains") shows that
   the Dedicated PHY (a separate IP block on the flash
   interface) has its own ``reg_pclk`` for APB access; the
   spreadsheet's ``regPCLK 125 MHz lsp_sp_clk`` row applies to
   that Dedicated PHY APB, **not** to the controller's reg-clk
   slot. The controller's register-access path rides ``mACLK``
   on ``lsp_mp_clk`` at 250 MHz, same lane as ``xspi_clk``.

   Linux binds ``xspi@9008000`` accordingly:

   .. code-block:: dts

       clocks      = <&clkmgr AGILEX72_LSP_MP_CLK>,
                     <&clkmgr AGILEX72_XSPI_CLK>;
       clock-names = "reg-clk", "core-clk";

   The U-Boot binding header adds ``AGILEX72_XSPI_CLK`` as
   an alias of ``AGILEX72_LSP_MP_CLK`` (same integer ID;
   same physical lane). ``socfpga_km.dtsi`` rebinds
   ``xspi@9008000`` to the same two-clock + clock-names form as
   Linux. The legacy ``xspi_clk: xspi-clk { fixed-clock
   @200MHz }`` placeholder in the dtsi's local
   ``clocks { ... }`` block is removed.

   The U-Boot Cadence XSPI driver
   (``drivers/spi/cadence_xspi.c``) does not consume the
   ``clocks`` property today, so this is forward-compat at
   probe time; the binding matches Linux for cross-tree
   review consistency.

   ``AGILEX72_XSPIPHY_CLK`` (200 MHz, GPPLL0_C3/10) remains
   a distinct ID for the Dedicated PHY's main ``clk_phy`` lane
   (HAS-GPPLL Section 9.8.8). No consumer binds it today
   because there is no separate ``phy@`` node in either U-Boot
   or Linux.

   The SDM-side ``sdm_xspi@18C00000`` is intentionally not
   migrated -- the SDM region is clocked by the SDM's own clock
   tree, not the HPS Clock Manager, and Linux does not
   include this node. The local ``sdm_xspi_clk:
   sdm-xspi-clk { fixed-clock@200MHz }`` node is the intended
   terminal binding.

F10 (rate-constants cross-validated against Linux runtime CSR reads)
   ``drivers/clk/altera/clk-agilex72.h`` still carries the v0.83
   default-preset rates from HAS-GPPLL Rev 0.83 Table 68 as
   compile-time fallbacks. After SPL handoff, ``clk_get_rate()``
   consults ``struct agilex72_clkmgr_rate_state`` (VCO + C-divider
   fields from the handoff interpreter) and decodes live CLKMGR-top
   CTR / NoCDIV fields where the topology requires it; the header
   constants apply only when no handoff has run or ``rate_state.valid``
   is false.

   Linux's ``drivers/clk/socfpga/clk-agilex72.c``
   takes the opposite approach: every ``recalc_rate`` reads the
   live CSR field and divides the parent rate, so the kernel
   reflects whatever SPL programmed regardless of source-tree
   constants. That makes Linux a useful cross-check oracle for the
   **default preset**: when a compile-time fallback constant
   disagrees with Linux's runtime read before handoff is applied,
   U-Boot is the side that is wrong.

   Four such mismatches were corrected against Linux's
   ``gate_clks[]`` tuples and the matching HAS-GPPLL Tables 18,
   46, 47, 50, 54, 59 and 68 entries:

   .. code-block:: text

       Constant                       Before          After
       AGILEX72_HSP_MP_HZ         GPPLL0_C0 / 10  GPPLL0_C0 / 2  (500 MHz)
                                      = 100 MHz       per HAS Table 54: mpclk=1 -> 1<<1=/2
       AGILEX72_LSP_MP_HZ         GPPLL0_C1       GPPLL0_C1 / 2  (250 MHz)
                                      = 500 MHz       per HAS Table 68: lsp_mp_clk = lspnoc/2
       AGILEX72_USB31_SUSPEND_HZ  25000000        USB31_REF_HZ   (20 MHz)
                                      hardcoded       per HAS Table 47 + Linux fixed_div=1
       AGILEX72_SDMMC_HZ          GPPLL0_C1 / 4   MEMPHY_HZ      (200 MHz)
                                      = 125 MHz       per HAS Table 18: sdmclk from
                                                      memdevice_phy_clk (GPPLL0_C3 / 10)

   The ``mmc0/1.biu`` binding remains on ``LSP_MP_CLK`` (now
   correctly 250 MHz), while ``mmc0/1.ciu`` rides
   ``SDMMC[01]_SDMCLK`` (now correctly 200 MHz). This separation
   matches HAS Table 18's split between the controller-side
   ``s_pclk`` / ``clk`` (lsp_mp_clk) and the Flash-domain
   ``sdmclk`` (memdevice_phy_clk).

   No GPIO debounce ("gpio_db_clk") correction was needed --
   Linux sets ``fixed_div=1`` for ``gpio_db_clk`` over
   ``gpio_db_free_clk``, yielding 250 MHz at the Clock Manager
   output (the further ``/2`` in HAS Table 68's "gpio_db_clk
   125 MHz" row is performed inside the GPIO IP's debounce
   prescaler, not in the CM). U-Boot's 250 MHz is therefore
   the correct CM-output rate and matches Linux.

   This documents the U-Boot side's drift-vs-truth audit
   methodology and the result is logged for future HAS-rev
   migrations: when a header constant disagrees with Linux's
   runtime CSR read for the same default preset, the Linux
   side wins.

F11 (handoff-driven per-design rates)
   The compile-time constants in ``clk-agilex72.h`` match the
   v0.83 default-preset GPPLL VCO targets (2000 / 1850 / 2500 MHz)
   and their fixed downstream topology. When a Quartus customer-
   choices change scales a PLL or C-divider away from that preset,
   ``clk_get_rate()`` must track what SPL programmed, not the
   source-tree constant.

   Linux solves this by decoding every CSR field in
   ``recalc_rate``. U-Boot solves it differently: the handoff
   interpreter (``drivers/clk/altera/agilex72-clkmgr.c``) populates
   ``struct agilex72_clkmgr_rate_state`` with the three PLL VCO
   frequencies (optional ``gppllN_freq`` KV overrides, CSR decode
   after ``pll_wait_lock``) and per-output C-divider values
   (``gppllN_cM_div`` KV keys plus ``agilex72_clkmgr_refresh_c_from_csr()``
   on GPPLL cfg_9..cfg_12 after lock). The DM clock driver consults
   that state at every ``get_rate`` call and walks CLKMGR-top CTR /
   NoCDIV CSRs for multi-stage peripheral lanes; it falls back to
   the compile-time defaults when no handoff has been applied
   (mis-configured build without ``HANDOFF_DEMO``,
   ``HANDOFF_EMBED_DEMO``, or a future on-silicon OCRAM producer).

   The architectural reasoning behind this split:

   - Quartus is the source of truth for what U-Boot programmed
     into the CLKMGR / GPPLL CSRs. The handoff is the explicit
     producer/consumer contract; duplicating full GPPLL CSR decode
     in the DM driver would either repeat that information or
     contradict it.
   - The U-Boot DM clock driver is exercised by a handful of
     consumers (dwmmc, dwc_eth_xgmac, dwc2 OTG, dwc3, cadence_xspi,
     i3c). None are dynamic-frequency consumers: they read the rate
     once at probe. A full Linux-style ``recalc_rate`` on every
     GPPLL CSR field would buy little beyond the handoff snapshot
     plus the CLKMGR-top reads already performed for gated clocks.
   - Once SPL has programmed the CSRs and U-Boot proper takes
     over, the CSRs are immutable for the rest of the boot.
     Cached rates from the handoff plus targeted CTR readback are
     equivalent to live CSR polling for boot-time consumers.

   Runtime self-audit (``AGILEX72_CONSUMER_AUDIT`` in
   ``clock_manager_agilex72.c``, gated on
   ``CONFIG_AGILEX72_CLKMGR_RUNTIME_AUDIT``) cross-checks
   ``clk_get_rate()`` against the same v0.83 default-preset
   goldens on the production handoff path (Simics 6595+ with
   ``HANDOFF_DEMO``; no ``SKIP_LOCK`` workaround).

Architect-confirmed clarifications
----------------------------------

This subsection records resolved ambiguities where the HAS
documentation disagreed with itself and the HW architect was
asked to pick the canonical artefact. Each entry captures the
question, the verdict, the affected in-tree code, and the
artefact that will be refreshed in the next HAS revision.

C1 (osc1timer{0,1} pclk source)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The two timer instances enabled by ``peripllgrp.ennoc[9:8]``
(``osc1timer{0,1}clken``, i.e. the ``L4SYSTIMER{0,1}`` reset
domains, mapped to ``timer2`` / ``timer3`` in
``arch/arm/dts/socfpga_km.dtsi``) had four v0.83 artefacts that
did not agree on the pclk source:

* Figure 9-16 *LS PSS Clock Muxes, Dividers, and Gates* drew
  an "OSC1 Timer Wrapper" block on the ``lsp_sp_clk`` rail
  alongside the SP-timer wrappers.
* Table 9-14 *LSP Clocks*, the V9 register sheet
  (``peripllgrp.ennoc`` at offset 0xE0) and the Agilex 72 HPS Clocks
  spreadsheet all pointed at ``lsp_sys_free_clk``.

Architect verdict: ``lsp_sys_free_clk``. Table 9-14, the V9
register sheet and the spreadsheet are canonical for v0.83;
Figure 9-16's lower wrapper block is a labelling bug and has
been flagged for refresh in the next HAS revision.

Code state: ``timer2`` / ``timer3`` in ``socfpga_km.dtsi``
bind to ``AGILEX72_LSP_SYS_FREE_CLK`` per the architect verdict;
no further DTS change is required.

Affected next-rev HAS items: Figure 9-16 lower-rail label.

C2 (CLKMGR demo-handoff cookie words: V9 silicon)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The in-tree demo handoff payload (demo C-array in
``drivers/clk/altera/agilex72-clkmgr-demo-handoff.c``) programs
two CLKMGR-top cookie words per the V9 register sheet:

* ``0x0915C030`` ``mainpllgrp.en`` — DV cookie ``0xDE0`` (open item:
  architect sheet lists ``0xDC0``; bit 5 pending confirmation)
* ``0x0915C03C`` ``mainpllgrp.bypass`` — ``0x004`` (APSPERIPH only)

The pre-V9 (A76) cookie words (``0xFE0`` / ``0x180``) and the
hardcoded ``spl_km.c::clk_mgr_init()`` path were removed once Agilex 72
committed to V9 (A720+A520) silicon exclusively.

Architect verdict (received 2026-05-25, JYK): the V9 register sheet
is canonical; ``KM_HPS_PLL_Reg_Audit.xlsx`` is no longer authoritative.

Code state: single demo-handoff source selected by
``CONFIG_AGILEX72_CLKMGR_HANDOFF_DEMO=y``. PLL presets are
sourced from DV SYSPRESET0 bin1 (GPPLL0 2000 MHz, GPPLL1 1850 MHz,
GPPLL2 2500 MHz VCO). Silicon validation pending — see open-item
list in the demo-handoff source file header.

1. PLL presets: confirm the 78 CSR values still meet A720/A520
   frequency targets.
2. ``mainpllgrp.en`` reserved bits 5/9 on V9 silicon: RAZ/WI, or
   side-effect-bearing? Check with emulation team.
3. ``mainpllgrp.bypass`` reserved bits 7/8 on V9 silicon: same
   question.
4. ``peripllgrp.en`` permissive default (``0xFFFFFFFF``) should
   be trimmed to the actual V9 NoC peripheral list.

Affected next-rev HAS items: HAS Section 9.1 "Register
Definition" pointer (and all ``KM_HPS_PLL_Reg_Audit.xlsx``
references throughout the HAS) to be redirected to the V9
register sheet.

Push-back templates
-------------------

These are ready-to-paste comments for Linux upstream coordination
threads. Use the matching one when a reviewer asks why U-Boot and Linux
spell something differently.

Template A: GPPLL1 naming asymmetry
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    Both trees use AGILEX72_GPPLL1_C0_CLK at integer ID 7, matching
    HAS Table 171 (GPPLL1's single output is the C0/Fclkout0 row).
    U-Boot also defines AGILEX72_GPPLL1_C1_CLK as a preprocessor
    alias of C0_CLK for legacy DTS spelling; it is not a second clock.
    See this document in the SoCFPGA U-Boot fork for the full policy.

Template B: v0.83 spelling aliases for shared integer IDs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    HAS v0.83 renames or splits a handful of clocks that Linux spells
    in v0.8 form (XSPIPHY_* vs XSPI_PHY_*,
    SDMMC{0,1}_CLK vs SDMMC{0,1}_SDMCLK, USB31 REF/SUSPEND
    folded onto one wire). The U-Boot Agilex 72 dt-binding header
    uses the v0.83 names and aliases each one to the same
    integer ID Linux assigns to the v0.8 name. Consumers that
    use either spelling resolve to the same DTB integer.

    No Linux-side change is requested -- the v0.8/v0.83 spelling
    asymmetry is intentional. See this document in the SoCFPGA U-Boot
    fork for the full mapping table.

Template C: Compatible string drop
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

    U-Boot handoff series dropped the legacy "altr,km-clkmgr"
    fallback from the driver match table and from
    socfpga_km.dtsi. New Agilex 72 DTS should use only
    "altr,agilex72-clkmgr". Existing v0.8-era DTs that
    list "altr,km-clkmgr" first or only will need to be
    updated.

Source-of-truth references
--------------------------

* **HAS document (canonical for U-Boot v0.83)**: Agilex 72 HPS Clock
  Manager HAS-GPPLL, Rev 0.83, 13 February 2026.
* **Register sheet (canonical for U-Boot v0.83)**: New Agilex 72 HPS
  Clock Manager V9.tsv.
* **Linux HAS baseline**: HAS v0.8 (snapshot dated
  before SDMMC1 / USB31 splits and other v0.83 clock-tree updates
  landed).

If you are touching ``include/dt-bindings/clock/altr,agilex72-clock.h``
or the matching driver tables in ``drivers/clk/altera/clk-agilex72.c``,
keep this file in sync with the change.
