#!/usr/bin/env python

# Usage: python git-read-index-v5.py [-h] [-v] [--file=FILENAME]
# The -h command line option shows the header of the index file
# The -v command line option shows a more verbose file list
# The --file command takes an argument, which file should be read.
# The -h and -v options are mutually exclusive.
# If no argument is given, the output is a list of all files in th index file
# including the path, sorted lexically. (The same format as git ls-files)
# (including stat data)
#
# The index v2/v3 can be converted to v5 using git-convert-index.py

import struct
import binascii
import sys
from collections import deque

DIR_DATA_STRUCT = struct.Struct("!HIIIIII")
HEADER_STRUCT = struct.Struct("!IIII")
CRC_STRUCT = struct.Struct("!i")
OFFSET_STRUCT = struct.Struct("!I")
FILE_DATA_STRUCT = struct.Struct("!HHIII")


def read_calc_crc(f, n, partialcrc=0):
    data = f.read(n)
    crc = binascii.crc32(data, partialcrc)
    return data, crc


def read_header(f):
    (signature, partialcrc) = read_calc_crc(f, 4)
    (readheader, partialcrc) = read_calc_crc(f,
            HEADER_STRUCT.size, partialcrc)
    (vnr, ndir, nfile, nextensions) = HEADER_STRUCT.unpack(readheader)

    if signature != "DIRC" or vnr != 5:
        raise Exception("Signature or version of the index are wrong.\n"
                "Header: %(signature)s\tVersion: %(vnr)s" %
                {"signature": signature, "vnr": vnr})

    extoffsets = list()
    for i in xrange(nextensions):
        (readoffset, partialcrc) = read_calc_crc(f,
                CRC_STRUCT.size, partialcrc)
        extoffsets.append(readoffset)

    crc = f.read(4)
    datacrc = CRC_STRUCT.pack(partialcrc)

    if crc != datacrc:
        raise Exception("Wrong header crc")

    return dict(signature=signature, vnr=vnr, ndir=ndir, nfile=nfile,
            nextensions=nextensions, extoffsets=extoffsets)


def read_name(f, partialcrc=0):
    name = ""
    (byte, partialcrc) = read_calc_crc(f, 1, partialcrc)
    while byte != '\0':
        name += byte
        (byte, partialcrc) = read_calc_crc(f, 1, partialcrc)

    return name, partialcrc


def read_index_entries(f, header):
    # Skip header and directory offsets
    f.seek(24 + header["nextensions"] * 4 + header["ndir"] * 4)

    directories = read_dirs(f, header["ndir"])

    # The foffset only needs to be considered for the first directory, since
    # we read the files continously and have the file pointer always in the
    # right place. Doing so saves 2 seeks per directory.
    f.seek(directories[0]["foffset"])
    (readoffset, partialcrc) = read_calc_crc(f, OFFSET_STRUCT.size)
    (offset, ) = OFFSET_STRUCT.unpack(readoffset)
    f.seek(offset)

    files = list()
    read_files(f, directories, 0, files)
    return files


def read_file(f, pathname):
    # A little cheating here in favor of simplicity and execution speed.
    # The fileoffset is only read when really needed, in the other cases
    # it's just calculated from the file position, to save on reads and
    # simplify the code.
    partialcrc = binascii.crc32(struct.pack("!I", f.tell()))

    (filename, partialcrc) = read_name(f, partialcrc)

    (statdata, partialcrc) = read_calc_crc(f, FILE_DATA_STRUCT.size,
            partialcrc)
    (flags, mode, mtimes, mtimens,
            statcrc) = FILE_DATA_STRUCT.unpack(statdata)

    (objhash, partialcrc) = read_calc_crc(f, 20, partialcrc)

    datacrc = CRC_STRUCT.pack(partialcrc)
    crc = f.read(4)
    if datacrc != crc:
        raise Exception("Wrong CRC for file entry: " + filename)

    return dict(name=pathname + filename,
            flags=flags, mode=mode, mtimes=mtimes, mtimens=mtimens,
            statcrc=statcrc, objhash=binascii.hexlify(objhash))


def read_files(f, directories, dirnr, files_out):
    queue = deque()
    for i in xrange(directories[dirnr]["nfiles"]):
        queue.append(read_file(f, directories[dirnr]["pathname"]))

    while queue:
        if (len(directories) > dirnr + 1 and
                queue[0]["name"] > directories[dirnr + 1]["pathname"]):
            dirnr = read_files(f, directories, dirnr + 1, files_out)
        else:
            files_out.append(queue.popleft())

    return dirnr


def read_dir(f):
    (pathname, partialcrc) = read_name(f)

    (readstatdata, partialcrc) = read_calc_crc(f, DIR_DATA_STRUCT.size,
            partialcrc)
    (flags, foffset, cr, ncr, nsubtrees, nfiles,
            nentries) = DIR_DATA_STRUCT.unpack(readstatdata)

    (objname, partialcrc) = read_calc_crc(f, 20, partialcrc)

    datacrc = CRC_STRUCT.pack(partialcrc)
    crc = f.read(4)
    if crc != datacrc:
        raise Exception("Wrong crc for directory entry: " + pathname)

    return dict(pathname=pathname, flags=flags, foffset=foffset,
        cr=cr, ncr=ncr, nsubtrees=nsubtrees, nfiles=nfiles,
        nentries=nentries, objname=objname)


def read_dirs(f, ndir):
    dirs = list()
    for i in xrange(ndir):
        dirs.append(read_dir(f))

    return dirs


def print_header(header):
    print("Signature: %(signature)s\t\t\tVersion: %(vnr)s\n"
            "Number of directories: %(ndir)s\tNumber of files: %(nfile)s\n"
            "Number of extensions: %(nextensions)s" % header)


def print_directories(directories):
    for d in directories:
        print ("path: %(pathname)s flags: %(flags)s foffset: %(foffset)s "
                "ncr: %(ncr)s cr: %(cr)s nfiles: %(nfiles)s "
                "nentries: %(nentries)s objname: " % d +
                str(binascii.hexlify(d["objname"])))


def print_files(files, verbose=False):
    for fi in files:
        if verbose:
            print ("%(name)s (%(objhash)s)\nmtime: %(mtimes)s:%(mtimens)s\n"
                    "mode: %(mode)s flags: %(flags)s\nstatcrc: " % fi
                    + hex(fi["statcrc"]))
        else:
            print fi["name"]


def main(args):
    f = None
    pheader = False
    pverbose = False
    for arg in args:
        if arg == "-h":
            pheader = True
        if arg == "-v":
            pverbose = True
        if arg[:7] == '--file=':
            f = open(arg[7:], "rb")

    if not f:
        f = open(".git/index-v5", "rb")

    header = read_header(f)

    files = read_index_entries(f, header)
    if pheader:
        print_header(header)
    else:
        print_files(files, pverbose)

if __name__ == "__main__":
    main(sys.argv[1:])
