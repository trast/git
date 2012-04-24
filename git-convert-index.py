import socket
import struct

def ntohs(n):
    return str(struct.unpack('!i', n)[0])

f = open(".git/index", "rb")

try:
    # Signature
    signature = f.read(4)
    print "Signature: " + signature

    version = f.read(4)
    print "Version: " + ntohs(version)

    nrofentries = f.read(4)
    print "Number of index entries: " + ntohs(nrofentries)

    byte = f.read(1)

    i = 0
    while i < int(ntohs(nrofentries)):
        ctimesec = byte + f.read(3)
        # Next header (Cache tree extension)
        if ctimesec == "TREE":
            break
        print "ctime seconds: " + ntohs(ctimesec)

        ctimensec = f.read(4)
        print "ctime nanoseconds: " + ntohs(ctimensec)

        mtimesec = f.read(4)
        print "mtime seconds: " + ntohs(mtimesec)

        mtimensec = f.read(4)
        print "mtime nanoseconds: " + ntohs(mtimensec)

        dev = f.read(4)
        print "dev: " + ntohs(dev)

        ino = f.read(4)
        print "ino: " + ntohs(ino)

        mode = f.read(4)
        print "mode: " + ntohs(mode)

        uid = f.read(4)
        print "uid: " + ntohs(uid)

        gid = f.read(4)
        print "gid: " + ntohs(gid)

        filesize = f.read(4)
        print "Truncated file size: " + ntohs(filesize)
        
        sha1 = f.read(20)
        print "SHA1: not implemented yet"

        flags = f.read(2)
        print "Flags: " + str(struct.unpack('!h', flags)[0])

        if (ntohs(version) == 3):
            xtflags = f.read(2)
            print "Extended flags: " + str(struct.unpack('!h', xtflags)[0])

        string = ""
        byte = f.read(1)
        while byte != '\0':
            string = string + byte
            byte = f.read(1)

        print string

        while byte == '\0':
            byte = f.read(1)

        i = i + 1

finally:
    f.close()
