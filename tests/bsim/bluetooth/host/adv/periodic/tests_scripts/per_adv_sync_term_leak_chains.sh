#!/usr/bin/env bash
# Copyright (c) 2026 Kent Hauser
# SPDX-License-Identifier: Apache-2.0

# As per_adv_sync_term_leak.sh, with BT_CTLR_SCAN_AUX_USE_CHAINS.

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="${BOARD_TS}_per_adv_sync_term_leak_chains"
verbosity_level=2
EXECUTE_TIMEOUT=120

cd ${BSIM_OUT_PATH}/bin

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_periodic_prj_sync_term_leak_chains_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=0 \
  -testid=per_adv_sync_term_leak_advertiser

Execute ./bs_${BOARD_TS}_tests_bsim_bluetooth_host_adv_periodic_prj_sync_term_leak_chains_conf \
  -v=${verbosity_level} -s=${simulation_id} -d=1 \
  -testid=per_adv_sync_term_leak

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
  -D=2 -sim_length=120e6 $@

wait_for_background_jobs
