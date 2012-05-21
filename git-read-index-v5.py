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
import python.lib.indexlib as indexlib
from collections import deque




def read_calc_crc(f, n, partialcrc=0):
    """ Reads a chunk of data and generates the crc sum for the data. The crc
    sum can also be combined with a crc code calculated earlier by using the
    partialcrc parameter

    Args:
        f: the file from which the data should be read
        n: number of bytes to read
        partialcrc: a earlier calculated crc, which should be taken into
            account when calculating the crc
    Returns:
        data, crc: a tuple of the read data and the calculated crc code
    """
    data = f.read(n)
    crc = indexlib.calculate_crc(data, partialcrc)
    return data, crc


def read_header(f):
    """ Read the header of a index-v5 file

    Args:
        f: the file from which the header should be read
    Returns:
        A dict with all the header values
    Raises:
        SignatureError: the signature of the index file is wrong
        VersionError: the version of the index file is wrong
        CrcError: the crc code doesn't match with the contents that have been
            read
    """
    # 4 byte signature
    (readheader, partialcrc) = read_calc_crc(f,
            indexlib.HEADER_V5_STRUCT.size)
    (signature, vnr, ndir, nfile,
            nextensions) = indexlib.HEADER_V5_STRUCT.unpack(readheader)

    if signature != "DIRC":
        raise indexlib.SignatureError("Signature is not DIRC. Signature: " +
                signature)

    if vnr != 5:
        raise indexlib.VersionError("The index is not Version 5. Version: " + str(vnr))

    extoffsets = list()
    for i in xrange(nextensions):
        (readoffset, partialcrc) = read_calc_crc(f,
                indexlib.CRC_STRUCT.size, partialcrc)
        extoffsets.append(readoffset)

    crc = f.read(indexlib.CRC_STRUCT.size)
    datacrc = indexlib.CRC_STRUCT.pack(partialcrc)

    if crc != datacrc:
        raise indexlib.CrcError("Wrong header crc")

    return dict(signature=signature, vnr=vnr, ndir=ndir, nfile=nfile,
            nextensions=nextensions, extoffsets=extoffsets)


def read_name(f, partialcrc=0):
    """ Read a nul terminated name from the file

    Args:
        f: the file from which the name should be read. The method will start
            reading from where the file pointer in that file is at the moment.
        partialcrc: A partial crc code of earlier read data, that should be
            taken into account.
    Returns:
        name, partialcrc: The name that was read and the crc code of the data
            that was read, taking into account a partial crc code if there is
            any.
    """
    name = ""
    (byte, partialcrc) = read_calc_crc(f, 1, partialcrc)
    while byte != '\0':
        name += byte
        (byte, partialcrc) = read_calc_crc(f, 1, partialcrc)

    return name, partialcrc


def read_index_entries(f, header):
    """ Read all index entries in the index file

    Args:
        f: the file from which the index entries should be read.
        header: the header of the index file
    Returns:
        A list of all index entries
    """
    # Skip header and directory offsets
    # Header size = 24 bytes, each extension offset and dir offset is 4 bytes
    f.seek(24 + header["nextensions"] * 4 + header["ndir"] * 4)

    directories = read_dirs(f, header["ndir"])

    # The foffset only needs to be considered for the first directory, since
    # we read the files continously and have the file pointer always in the
    # right place. Doing so saves 2 seeks per directory.
    f.seek(directories[0]["foffset"])
    (readoffset, partialcrc) = read_calc_crc(f, indexlib.OFFSET_STRUCT.size)
    (offset, ) = indexlib.OFFSET_STRUCT.unpack(readoffset)
    f.seek(offset)

    files = list()
    read_files(f, directories, 0, files)
    return files


def read_file(f, pathname):
    """ Read a single file from the index

    Args:
        f: the index file from which the file data should be read.
        pathname: the pathname of the file, with which the filename is
            combined to get the full path
    Returns:
        A dict with all filedata
    Raises:
        CrcError: The crc code in the index file doesn't match with the crc
            code of the read data.
    """
    # A little cheating here in favor of simplicity and execution speed.
    # The fileoffset is only read when really needed, in the other cases
    # it's just calculated from the file position, to save on reads and
    # simplify the code.
    partialcrc = binascii.crc32(struct.pack("!I", f.tell()))

    (filename, partialcrc) = read_name(f, partialcrc)

    (statdata, partialcrc) = read_calc_crc(f, indexlib.FILE_DATA_STRUCT.size,
            partialcrc)
    (flags, mode, mtimes, mtimens,
            statcrc, objhash) = indexlib.FILE_DATA_STRUCT.unpack(statdata)

    datacrc = indexlib.CRC_STRUCT.pack(partialcrc)
    crc = f.read(indexlib.CRC_STRUCT.size)
    if datacrc != crc:
        raise indexlib.CrcError("Wrong CRC for file entry: " + filename)

    return dict(name=pathname + filename,
            flags=flags, mode=mode, mtimes=mtimes, mtimens=mtimens,
            statcrc=statcrc, objhash=binascii.hexlify(objhash))


def read_files(f, directories, dirnr, files_out):
    """ Read all files from the index and combine them with their respective
    pathname. Files are read lexically ordered. The function does this
    recursively

    Args:
        f: the index file from which the files should be read
        directories: all directories that are in the index file
        dirnr: The files of the dirnrth directory are read. This has to be 0
            for the initial call.
        files_out: The list into which is used to store the files. Pass a
            empty list at the initial call. The list will contain all the files
            when the function returns.
    Returns:
        dirnr: The number of the directory that was used last. Of no use in the
            function that calls read_files
    """
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
    """ Read a single directory from the index file.

    Args:
        f: The index file from which the directory data should be read
    Returns:
        A dict with all file data
    Raises:
        CrcError: The crc code in the file doesn't match with the crc code
            of the data that was read
    """
    (pathname, partialcrc) = read_name(f)

    (readstatdata, partialcrc) = read_calc_crc(f,
            indexlib.DIRECTORY_DATA_STRUCT.size, partialcrc)
    (flags, foffset, cr, ncr, nsubtrees, nfiles, nentries,
            objname) = indexlib.DIRECTORY_DATA_STRUCT.unpack(readstatdata)

    datacrc = indexlib.CRC_STRUCT.pack(partialcrc)
    crc = f.read(indexlib.CRC_STRUCT.size)
    if crc != datacrc:
        raise indexlib.CrcError("Wrong crc for directory entry: " + pathname)

    return dict(pathname=pathname, flags=flags, foffset=foffset,
        cr=cr, ncr=ncr, nsubtrees=nsubtrees, nfiles=nfiles,
        nentries=nentries, objname=binascii.hexlify(objname))


def read_dirs(f, ndir):
    """ Read all directories from the index file.

    Args:
        f: The index file from which the directories should be read
        ndir: Number of directory that should be read
    Returns:
        A list of all directories in the index file
    """
    dirs = list()
    for i in xrange(ndir):
        dirs.append(read_dir(f))

    return dirs


def print_header(header):
    print(indexlib.HEADER_V5_FORMAT % header)


def print_directories(directories):
    for d in directories:
        print (indexlib.DIRECTORY_FORMAT % d)


def print_files(files, verbose=False):
    for fi in files:
        if verbose:
            print (indexlib.FILES_FORMAT % fi + hex(fi["statcrc"]))
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
