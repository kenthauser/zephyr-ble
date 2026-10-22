#!/usr/bin/env python3
# Copyright (c) 2026 Kent Hauser
# SPDX-License-Identifier: Apache-2.0

"""Check auxiliary packet timing against the AuxPtr that announced it.

Reads an advertiser's 2G4 phy Tx dump (d_2G4_<n>.Tx.csv). For every
ADV_EXT_IND carrying an AuxPtr, finds the auxiliary packet on the announced
channel and checks its start time, as required by Core Vol 6, Part B,
Section 2.3.4.5: the auxiliary packet shall not start any earlier than the
Aux Offset and shall start no later than the Aux Offset plus one Offset Unit.
The offset is measured from the start of the packet holding the AuxPtr.

Exits non-zero on any violation, or if no auxiliary packet was checked.
"""

import csv
import sys

ADV_ACCESS_ADDRESS = 0x8E89BED6
PRIMARY_FREQS = (2.0, 26.0, 80.0)  # MHz above 2400: channels 37, 38, 39
PDU_TYPE_ADV_EXT_IND = 0x07
TOLERANCE_US = 1  # bsim time resolution

# Extended header fields in order: (flag bit, size in octets, name)
EXT_HDR_FIELDS = (
    (0, 6, 'adva'),
    (1, 6, 'targeta'),
    (2, 1, 'cteinfo'),
    (3, 2, 'adi'),
    (4, 3, 'auxptr'),
    (5, 18, 'syncinfo'),
    (6, 1, 'txpower'),
)


def channel_freq(chan_idx):
    """Data channel index to MHz above 2400."""
    return 4 + 2 * chan_idx if chan_idx <= 10 else 28 + 2 * (chan_idx - 11)


def ext_hdr_fields(pdu):
    """Return the extended header fields of a common extended advertising PDU."""
    if len(pdu) < 3 or (pdu[0] & 0x0F) != PDU_TYPE_ADV_EXT_IND:
        return None
    if (pdu[2] & 0x3F) == 0:
        return {}
    flags = pdu[3]
    pos = 4
    fields = {}
    for bit, size, name in EXT_HDR_FIELDS:
        if flags & (1 << bit):
            fields[name] = pdu[pos : pos + size]
            pos += size
    return fields


def load_tx(path):
    packets = []
    with open(path, newline='') as fh:
        for row in csv.DictReader(fh):
            if int(row['phy_address'], 16) != ADV_ACCESS_ADDRESS:
                continue
            pdu = bytes(int(octet, 16) for octet in row['packet'].split())
            packets.append((int(row['start_time']), float(row['center_freq']), pdu))
    return packets


def main(path):
    packets = load_tx(path)
    checked = 0
    violations = 0

    for idx, (start, freq, pdu) in enumerate(packets):
        if freq not in PRIMARY_FREQS:
            continue
        fields = ext_hdr_fields(pdu)
        if not fields or 'auxptr' not in fields:
            continue

        aux_ptr = fields['auxptr']
        unit_us = 300 if aux_ptr[0] & 0x80 else 30
        offset = (aux_ptr[1] | (aux_ptr[2] << 8)) & 0x1FFF
        expected = start + offset * unit_us
        aux_freq = channel_freq(aux_ptr[0] & 0x3F)

        aux_start = next(
            (t for t, f, _ in packets[idx + 1 :] if f == aux_freq and t >= expected - unit_us), None
        )
        if aux_start is None or aux_start > expected + 2 * unit_us:
            # Not sent (e.g. advertising stopped); nothing to check
            continue

        checked += 1
        late = aux_start - expected
        if late < -TOLERANCE_US or late > unit_us + TOLERANCE_US:
            violations += 1
            print(
                f"ADV_EXT_IND at {start} us: aux at {aux_start} us, "
                f"{late} us from the Aux Offset (allowed 0..{unit_us} us)"
            )

    print(
        f"Checked {checked} auxiliary packets against their AuxPtr, "
        f"{violations} outside the allowed window"
    )
    return 1 if (violations or not checked) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
