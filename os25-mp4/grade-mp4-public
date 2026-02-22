#!/usr/bin/env python3

import re
from gradelib import *

r = Runner(save("xv6.out"))

# Large Files Tests
@test(0, "Testing large files")
def test_bigfile():
    r.run_qemu(shell_script([
        'bigfile'
    ]), timeout=300)

@test(7, "Large files: public testcase 1 (7 points)", parent=test_bigfile)
def test_bigfile_public1():
    r.match('^public testcase 1: ok$')

@test(7, "Large files: public testcase 2 (7 points)", parent=test_bigfile)
def test_bigfile_public2():
    r.match('^public testcase 2: ok$')

# Symbolic Links to Files Tests
@test(0, "Testing symbolic links to files (public)")
def test_symlinkfile():
    r.run_qemu(shell_script([
        'symlinkfile'
    ]), timeout=20)

@test(7, "Symbolic links to files: public testcase 1 (7 points)", parent=test_symlinkfile)
def test_symlink_file_public1():
    r.match("^public testcase 1: ok$")

@test(7, "Symbolic links to files: public testcase 2 (7 points)", parent=test_symlinkfile)
def test_symlink_file_public2():
    r.match("^public testcase 2: ok$")

# Symbolic Links to Directories Tests
@test(0, "Testing symbolic links to directories (public)")
def test_symlinkdir():
    r.run_qemu(shell_script([
        'symlinkdir'
    ]), timeout=20)

@test(7, "Symbolic links to directories: public testcase 1 (7 points)", parent=test_symlinkdir)
def test_symlink_dir_public1():
    r.match("^public testcase 1: ok$")

@test(7, "Symbolic links to directories: public testcase 2 (7 points)", parent=test_symlinkdir)
def test_symlink_dir_public2():
    r.match("^public testcase 2: ok$")

run_tests() 