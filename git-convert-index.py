import socket
import struct

f = open(".git/index", "rb")

try:
    # Signature
    signature = f.read(4)
    print "Signature: " + signature

    version = f.read(4)
    print "Version: " + str(struct.unpack('!i',version)[0])

    nrofentries = f.read(4)
    print "Number of index entries: " + str(struct.unpack('!i',nrofentries)[0])
    
    #while byte != "":
    #    print byte
    #    byte = f.read(1)
finally:
    f.close()
