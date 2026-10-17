# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(bluetooth_ctrl_pdu_layout)

target_include_directories(testbinary PRIVATE
  ${ZEPHYR_BASE}/tests/bluetooth/controller/mock_ctrl/include
  ${ZEPHYR_BASE}/subsys/bluetooth/controller/include
  ${ZEPHYR_BASE}/subsys/bluetooth/controller
  ${ZEPHYR_BASE}/subsys/bluetooth/controller/ll_sw
  ${ZEPHYR_BASE}/subsys/bluetooth/controller/ll_sw/nordic
)

target_sources(testbinary PRIVATE
  src/main.c
)

# The meta scenarios supply their own vendor meta data header; no in-tree
# LLL enables PDU meta data (both lll_meta.h files #error on it).
if(CONFIG_BT_CTLR_RX_PDU_META OR CONFIG_BT_CTLR_TX_PDU_META)
  target_include_directories(testbinary BEFORE PRIVATE meta)
endif()
