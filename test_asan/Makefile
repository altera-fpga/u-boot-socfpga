# SPDX-License-Identifier: GPL-2.0+
#
# Top-level dispatcher for the native ASan/UBSan test harnesses.
#
# Layout:
#   test_asan/
#     Makefile          <-- this file (generic dispatcher)
#     <driver>/         <-- one directory per driver
#       Makefile        <-- implements: all, run, clean
#       <driver>_asan.c <-- harness source
#
# Adding a new driver harness requires no edits here: drop a new
# <driver>/ folder with its own Makefile and it is picked up on the
# next invocation.
#
# Usage (from this directory):
#   make            -- build + run every driver harness
#   make run        -- run every built harness
#   make clean      -- clean every driver harness
#   make <driver>   -- build + run a single driver (e.g. `make rsu`)

SUBDIRS := $(sort $(patsubst %/Makefile,%,$(wildcard */Makefile)))

.PHONY: all run clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@echo ""
	@echo "######## test_asan: $@ ########"
	@$(MAKE) --no-print-directory -C $@

run:
	@set -e; for d in $(SUBDIRS); do \
		echo ""; \
		echo "######## test_asan: run $$d ########"; \
		$(MAKE) --no-print-directory -C $$d run; \
	done

clean:
	@for d in $(SUBDIRS); do \
		$(MAKE) --no-print-directory -C $$d clean; \
	done
