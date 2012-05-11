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
    signature = fread(4)
    header = struct.unpack('!IIII', fread(16))
    crc = f.read(4)
    readcrc = struct.pack("!i", binascii.crc32("".join(filedata)))
    if crc == readcrc:
        return dict({"signature": signature, "vnr": header[0], "ndir": header[1], "nfile": header[2], "next": header[3]})
    else:
        raise Exception("Wrong crc")


def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["vnr"])
    print "Number of directories: " + str(header["ndir"])
    print "Number of files: " + str(header["nfile"])
    print "Number of extension: " + str(header["next"])


header = readheader(f)




printheader(header)
