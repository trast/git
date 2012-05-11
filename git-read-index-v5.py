#!/usr/bin/env python

import struct
import binascii

f = open(".git/index-v5", "rb")
filedata = list()


def fread(n):
    global filedata
    data = f.read(n)
    filedata.append(data)
    return data


def readheader(f):
    global filedata
    signature = fread(4)
    header = struct.unpack('!IIII', fread(16))
    crc = f.read(4)
    readcrc = struct.pack("!i", binascii.crc32("".join(filedata)))
    filedata = list()
    if crc == readcrc:
        return dict({"signature": signature, "vnr": header[0], "ndir": header[1], "nfile": header[2], "next": header[3]})
    else:
        raise Exception("Wrong crc")


def readindexentries(header):
    f.seek(24 + header["ndir"] * 4)
    directories = readdirs(header["ndir"])
    files, dirnr = readfiles(directories, 0, [])
    for fi in files:
        print fi["name"]
    # printdirectories(directories)


def readfiles(directories, dirnr, entries):
    global filedata
    f.seek(directories[dirnr]["foffset"])
    offset = struct.unpack("!I", fread(4))[0]
    f.seek(offset)
    filedata = list()
    queue = list()
    i = 0
    while i < directories[dirnr]["nfiles"]:
        filedata.append(struct.pack("!I", f.tell()))
        filename = ""
        byte = fread(1)
        while byte != '\0':
            filename += byte
            byte = fread(1)

        data = struct.unpack("!HHIII", fread(16))
        objhash = fread(20)
        readcrc = struct.pack("!i", binascii.crc32("".join(filedata)))
        crc = f.read(4)
        if readcrc != crc:
            print "Wrong CRC: " + filename
        filedata = list()

        i += 1

        queue.append(dict({"name": directories[dirnr]["pathname"] + filename, "flags": data[0], "mode": data[1], "mtimes": data[2], "mtimens": data[3], "statcrc": data[4], "objhash": binascii.hexlify(objhash)}))

    if len(directories) > dirnr:
        i = 0
        while i < len(queue):
            if len(directories) - 1 > dirnr and queue[i]["name"] > directories[dirnr + 1]["pathname"]:
                entries, dirnr = readfiles(directories, dirnr + 1, entries)
            else:
                entries.append(queue[i])
                i += 1
        return entries, dirnr


def readdirs(ndir):
    global filedata
    i = 0
    dirs = list()
    while i < ndir:
        pathname = ""
        byte = fread(1)
        while byte != '\0':
            pathname += byte
            byte = fread(1)

        data = struct.unpack("!HIIIIII", fread(26))
        objname = fread(20)

        readcrc = struct.pack("!i", binascii.crc32("".join(filedata)))
        crc = f.read(4)
        filedata = list()
        if crc != readcrc:
            raise Exception("Wrong crc for " + pathname)

        dirs.append(dict({"pathname": pathname, "flags": data[0], "foffset": data[1], "cr": data[2], "ncr": data[3], "nsubtrees": data[4], "nfiles": data[5], "nentries": data[6], "objname": objname}))
        i += 1

    return dirs


def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["vnr"])
    print "Number of directories: " + str(header["ndir"])
    print "Number of files: " + str(header["nfile"])
    print "Number of extension: " + str(header["next"])


def printdirectories(directories):
    for d in directories:
        print d["pathname"] + " " + str(d["flags"]) + " " + str(d["foffset"]) + " " + str(d["cr"]) + " " + str(d["ncr"]) + " " + str(d["nsubtrees"]) + " " + str(d["nfiles"]) + " " + str(d["nentries"]) + " " + str(binascii.hexlify(d["objname"]))


header = readheader(f)
readindexentries(header)

# printheader(header)
