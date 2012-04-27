import socket
import binascii
import struct

def convert(n):
    return str(struct.unpack('!I', n)[0])

f = open(".git/index", "rb")
fw = open(".git/index-v4", "wb")

# Signature
#fw.write(signature)
print "Signature: " + f.read(4)

header = struct.unpack('!II', f.read(8))
#fw.write(version)
print "Version: " + str(header[0])

#fw.write(nrofentries)
print "Number of index entries: " + str(header[1])

indexentries = []
byte = f.read(1)
i = 0
# Read index entries
while i < header[1]:
    entry = struct.unpack('!IIIIIIIIII', byte + f.read(39)) # stat data
    entry = entry + (str(binascii.hexlify(f.read(20))),)    # SHA-1

    if (header[0] == 3):
        entry = entry + struct.unpack('!hh', f.read(4))     # Flags + extended flags
    else:
        entry = entry + struct.unpack('!h', f.read(2))      # Flags

    string = ""
    byte = f.read(1)
    while byte != '\0':
        string = string + byte
        byte = f.read(1)

    entry = entry + (string, )                              # Filename

    if (header[0] == 3):
        dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
            'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
            'xtflags', 'filename'), entry))
    else:
        dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
            'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
            'filename'), entry))

    while byte == '\0':
        byte = f.read(1)

    indexentries.append(dictentry)

    i = i + 1

sup = f.read(3)
byte = byte + sup

print "Extension: " + byte

#if byte == "TREE":
if 0:                             # Not reading Tree extension
    extensionsize = f.read(4)
    print "Extensionsize: " + convert(extensionsize)


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



# Output
for entry in indexentries:
    print entry["filename"]
    print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
    print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
    print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
    print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
    print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]

sha1 = f.read(20)
print "SHA1 over the whole file: " + str(binascii.hexlify(sha1))
