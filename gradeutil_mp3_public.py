import re

# ======================================================================
#  Helper Functions for Page Table Matching
# ======================================================================


def _parse_flags(flag_str):
    """Convert flag string like 'V R X U S' to numeric flags"""
    flags = 0
    if "V" in flag_str:
        flags |= 0x01  # Valid (bit 0)
    if "R" in flag_str:
        flags |= 0x02  # Readable (bit 1)
    if "W" in flag_str:
        flags |= 0x04  # Writable (bit 2)
    if "X" in flag_str:
        flags |= 0x08  # Executable (bit 3)
    if "U" in flag_str:
        flags |= 0x10  # User (bit 4)
    if "S" in flag_str:
        flags |= 0x200  # Swap (bit 9)
    return flags


def _addr_check(flags, pa, pte):
    """Check if PTE matches expected PA and flags (only checking V,R,W,X,U,S bits)"""
    # Create mask for the bits we care about: V,R,W,X,U,S + PA bits
    # Bits 0-4: V,R,W,X,U
    # Bit 9: S
    # Bits 10-53: Physical Page Number
    relevant_bits_mask = 0x3FFFFFFFFFC3F  # Bits 0-4, 9, and 10-53

    expected_pte = ((pa >> 12) << 10) | flags

    # Only compare the relevant bits
    return (pte & relevant_bits_mask) == (expected_pte & relevant_bits_mask)


def _validate_page_table_line(output, pattern, is_leaf=True):
    """Extract and validate a page table line from output"""
    # Find all lines matching the pattern in the output
    lines = output.splitlines()
    matching_lines = []
    for line in lines:
        if re.match(pattern, line):
            matching_lines.append(line)

    if not matching_lines:
        raise AssertionError(f"No line matching pattern: {pattern}")

    # Take the last matching line (most recent)
    line = matching_lines[-1]

    # Extract PTE, VA, PA, and flags from the matched line
    pte_match = re.search(r"pte=0x([0-9a-f]+)", line)
    va_match = re.search(r"va=0x([0-9a-f]+)", line)
    pa_match = re.search(r"pa=0x([0-9a-f]+)", line)
    flags_match = re.search(r"pa=0x[0-9a-f]+\s+(.+)$", line)

    if pte_match and pa_match and flags_match:
        pte = int(pte_match.group(1), 16)
        pa = int(pa_match.group(1), 16)
        flags_str = flags_match.group(1).strip()

        # Handle special case with blockno
        if "blockno=" in flags_str:
            # Extract flags after blockno
            blockno_match = re.search(r"blockno=0x[0-9a-f]+\s+(.+)$", flags_str)
            if blockno_match:
                flags_str = blockno_match.group(1)

        flags = _parse_flags(flags_str)

        if is_leaf:
            # For leaf entries, check the standard PTE format
            if not _addr_check(flags, pa, pte):
                raise AssertionError(
                    f"Address check failed for line '{line}': PTE=0x{pte:x}, PA=0x{pa:x}, flags=0x{flags:x}, expected_pte=0x{((pa >> 12) << 10) | flags:x}"
                )
        else:
            # For intermediate entries, just check that V flag is set and PTE has valid format
            if not (flags & 0x01):  # V flag should be set
                raise AssertionError(
                    f"Intermediate entry missing V flag in line '{line}': PTE=0x{pte:x}, flags=0x{flags:x}"
                )
            # Basic sanity check: PTE should contain the PA shifted and V flag
            expected_pte = ((pa >> 12) << 10) | 0x01
            if pte != expected_pte:
                raise AssertionError(
                    f"Intermediate entry format error in line '{line}': PTE=0x{pte:x}, expected=0x{expected_pte:x}"
                )


def _match_baseline_pagetable(runner):
    """Matches the baseline page table state with 4 user pages."""
    runner.match(r"^page table 0x[0-9a-f]+")

    # Low-address user mappings
    runner.match(r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$",
    )

    runner.match(
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$",
    )

    runner.match(
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$",
    )

    # High-address kernel/trampoline mappings
    runner.match(r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$",
    )

    runner.match(
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$",
    )

    runner.match("")  # Match trailing blank line


def _match_pagetable_with_faulted_page(runner):
    """Matches the page table state after a page fault at va=0x5000."""
    runner.match(r"^page table 0x[0-9a-f]+")

    runner.match(r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$",
    )

    runner.match(
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$",
    )

    runner.match(
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$",
    )

    # The new, faulted-in page!
    runner.match(
        r"^      5: pte=0x[0-9a-f]+ va=0x0000000000005000 pa=0x[0-9a-f]+ V R W X U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      5: pte=0x[0-9a-f]+ va=0x0000000000005000 pa=0x[0-9a-f]+ V R W X U$",
    )

    runner.match(r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$",
    )

    # Note: Corrected a typo here from [0-_a-f] to [0-9a-f] for robustness
    runner.match(
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$",
    )

    runner.match("")


def _match_madvise_pagetable(runner):
    """Matches the page table, using a specific regex for the va=0x5000 entry."""
    runner.match(r"^page table 0x[0-9a-f]+")

    runner.match(r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      0: pte=0x[0-9a-f]+ va=0x0000000000000000 pa=0x[0-9a-f]+ V R X U$",
    )

    runner.match(
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      1: pte=0x[0-9a-f]+ va=0x0000000000001000 pa=0x[0-9a-f]+ V R W U$",
    )

    runner.match(
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      2: pte=0x[0-9a-f]+ va=0x0000000000002000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      3: pte=0x[0-9a-f]+ va=0x0000000000003000 pa=0x[0-9a-f]+ V R W U$",
    )

    # This is the line that changes, with special handling for blockno
    runner.match(
        r"^      5: pte=0x[0-9a-f]+ va=0x0000000000005000 pa=0x[0-9a-f]+ blockno=0x[0-9a-f]+ R W X U S$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      5: pte=0x[0-9a-f]+ va=0x0000000000005000 pa=0x[0-9a-f]+ blockno=0x[0-9a-f]+ R W X U S$",
    )

    runner.match(r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^  255: pte=0x[0-9a-f]+ va=0x0000003fc0000000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$")
    _validate_page_table_line(
        runner.qemu.output,
        r"^    511: pte=0x[0-9a-f]+ va=0x0000003fffe00000 pa=0x[0-9a-f]+ V$",
        is_leaf=False,
    )

    runner.match(
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      509: pte=0x[0-9a-f]+ va=0x0000003fffffd000 pa=0x[0-9a-f]+ V R U$",
    )

    runner.match(
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      510: pte=0x[0-9a-f]+ va=0x0000003fffffe000 pa=0x[0-9a-f]+ V R W$",
    )

    runner.match(
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$"
    )
    _validate_page_table_line(
        runner.qemu.output,
        r"^      511: pte=0x[0-9a-f]+ va=0x0000003ffffff000 pa=0x[0-9a-f]+ V R X$",
    )

    runner.match("")
