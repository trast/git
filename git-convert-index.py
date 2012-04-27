import socket
import binascii
import struct

def ntohs(n):
    return str(struct.unpack('!I', n)[0])

f = open(".git/index", "rb")
fw = open(".git/index-v4", "wb")

try:
    # Signature
    signature = f.read(4)
    fw.write(signature)
    print "Signature: " + signature

    version = f.read(4)
    fw.write(version)
    print "Version: " + ntohs(version)

    nrofentries = f.read(4)
    fw.write(nrofentries)
    print "Number of index entries: " + ntohs(nrofentries)

    byte = f.read(1)

    i = 0
    # Read index entries
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
        print "SHA1: " + str(binascii.hexlify(sha1))

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

    sup = f.read(3)
    byte = byte + sup

    print "Extension: " + byte

    if byte == "TREE":
        extensionsize = f.read(4)
        print "Extensionsize: " + ntohs(extensionsize)


        while 1:
            string = ""
            byte = f.read(1)
            while byte != '\0':
                string = string + byte
                byte = f.read(1)

            print "Path component: " + string

            string = ""
            byte = f.read(1)
            while byte != " ":
                string = string + byte
                byte = f.read(1)

            print "Entry_count: " +  string

            string = ""
            byte = f.read(1)
            while byte != "\n":
                string = string + byte
                byte = f.read(1)

            print "Number of subtrees: " + string

            sha1 = f.read(20)
            print "160-bit object name: " + str(binascii.hexlify(sha1))

    sha1 = f.read(20)
    print "SHA1 over the whole file: " + str(binascii.hexlify(sha1))

finally:
    f.close()
