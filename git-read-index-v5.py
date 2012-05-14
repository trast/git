#!/usr/bin/env python

# Outputs: All filenames (including path), sorted lexically. Output format is
# the same as git ls-files
#
# Usage: python git-read-index-v5.py
# Convert index v2/v3 with git-convert-index.py

import struct
import binascii

f = open(".git/index-v5", "rb")


def read_calc_crc(n, partialcrc=0):
    data = f.read(n)
    crc = binascii.crc32(data, partialcrc)
    return data, crc


def read_header(f):
    global filedata
    signature, partialcrc = read_calc_crc(4)
    readheader, partialcrc = read_calc_crc(16, partialcrc)
    vnr, ndir, nfile, nextensions = struct.unpack('!IIII', readheader)

    extoffsets = list()
    for i in xrange(0, nextensions):
        readoffset, partialcrc = read_calc_crc(4, partialcrc)
        extoffsets.append(readoffset)

    crc = f.read(4)
    datacrc = struct.pack("!i", partialcrc)

    if crc == datacrc:
        return dict(signature=signature, vnr=vnr, ndir=ndir, nfile=nfile,
                nextensions=nextensions, extoffsets=extoffsets)
    else:
        raise Exception("Wrong crc")


def readindexentries(header):
    # Skip header and directory offsets
    f.seek(24 + header["nextensions"] * 4 + header["ndir"] * 4)

    directories = read_dirs(header["ndir"])
    files, dirnr = readfiles(directories, 0, [])
    for fi in files:
        print fi["name"]


def readfiles(directories, dirnr, entries):
    if dirnr == 0:
        f.seek(directories[dirnr]["foffset"])
        (readoffset, partialcrc) = read_calc_crc(4)
        (offset, ) = struct.unpack("!I", readoffset)
        f.seek(offset)

    queue = list()
    for i in xrange(0, directories[dirnr]["nfiles"]):
        # A little cheating here in favor of simplicity and execution speed.
        # The fileoffset is only read when really needed, in the other cases
        # it's just calculated from the file position, to save on reads and
        # simplify the code.

        if partialcrc != 0:
            partialcrc = binascii.crc(struct.pack("!I", f.tell()))

        filename = ""
        (byte, partialcrc) = read_calc_crc(1, partialcrc)
        while byte != '\0':
            filename += byte
            (byte, partialcrc) = read_calc_crc(1, partialcrc)

        (statdata, partialcrc) = read_calc_crc(16, partialcrc)
        (flags, mode, mtimes, mtimens,
                statcrc) = struct.unpack("!HHIII", statdata)

        (objhash, partialcrc) = read_calc_crc(20, partialcrc)

        datacrc = struct.pack("!i", partialcrc)
        crc = f.read(4)
        if datacrc != crc:
            raise Exception("Wrong CRC: " + filename)

        queue.append(dict(name=directories[dirnr]["pathname"] + filename,
            flags=flags, mode=mode, mtimes=mtimes, mtimens=mtimens,
            statcrc=statcrc, objhash=binascii.hexlify(objhash)))

        partialcrc = 0

    if len(directories) > dirnr:
        i = 0
        while i < len(queue):
            if (len(directories) - 1 > dirnr and
                    queue[i]["name"] > directories[dirnr + 1]["pathname"]):
                entries, dirnr = readfiles(directories, dirnr + 1, entries)
            else:
                entries.append(queue[i])
                i += 1

        return entries, dirnr


def read_dirs(ndir):
    dirs = list()
    for i in xrange(0, ndir):
        pathname = ""
        (byte, partialcrc) = read_calc_crc(1)
        while byte != '\0':
            pathname += byte
            (byte, partialcrc) = read_calc_crc(1, partialcrc)

        (readstatdata, partialcrc) = read_calc_crc(26, partialcrc)
        (flags, foffset, cr, ncr, nsubtrees, nfiles,
                nentries) = struct.unpack("!HIIIIII", readstatdata)

        (objname, partialcrc) = read_calc_crc(20, partialcrc)

        datacrc = struct.pack("!i", partialcrc)
        crc = f.read(4)
        if crc != datacrc:
            raise Exception("Wrong crc for " + pathname)

        dirs.append(dict(pathname=pathname, flags=flags, foffset=foffset,
            cr=cr, ncr=ncr, nsubtrees=nsubtrees, nfiles=nfiles,
            nentries=nentries, objname=objname))

    return dirs


def printheader(header):
    print("Signature: %(signature)s\t\t\tVersion: %(vnr)s\n"
            "Number of directories: %(ndir)s\tNumber of files: %(nfile)s\n"
            "Number of extensions: %(nextensions)s" % header)


def printdirectories(directories):
    for d in directories:
        print ("path: %(pathname)s flags: %(flags)s foffset: %(foffset)s "
                "ncr: %(ncr)s cr: %(cr)s nfiles: %(nfiles)s "
                "nentries: %(nentries)s objname: " % d +
                str(binascii.hexlify(d["objname"])))


header = read_header(f)
# printheader(header)

if header["signature"] == "DIRC" and header["vnr"] == 5:
    directories = readindexentries(header)
else:
    raise Exception("Signature or version of the index are wrong.\n"
            "Header: %(signature)s\tVersion: %(vnr)s" % header)
