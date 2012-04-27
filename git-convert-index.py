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
extensiondata = []

if byte == "TREE":
    extensionsize = f.read(4)
    print "Extensionsize: " + convert(extensionsize)

    read = 0
    subtreenr = [0]
    subtree = [""]
    listsize = 0
    while read < int(convert(extensionsize)):
        path = ""
        byte = f.read(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = f.read(1)
            read += 1

        while listsize >= 0 and subtreenr[listsize] == 0:
            subtreenr.pop()
            subtree.pop()
            listsize -= 1

        print subtreenr
        print subtree

        fpath = ""
        if listsize > 0: 
            for p in subtree:
                if p != "": 
                    fpath += p + "/"
            subtreenr[listsize] = subtreenr[listsize] - 1
        fpath += path + "/"


        entry_count = ""
        byte = f.read(1)
        read += 1
        while byte != " ":
            entry_count += byte
            byte = f.read(1)
            read += 1

        subtrees = ""
        byte = f.read(1)
        read += 1
        while byte != "\n":
            subtrees += byte
            byte = f.read(1)
            read += 1

        subtreenr.append(int(subtrees))
        subtree.append(path)
        listsize += 1

        if entry_count != -1:
            sha1 = f.read(20)
            read += 20
        else:
            sha1 = "invalid"

        extensiondata.append(dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees, "sha1": binascii.hexlify(sha1)}))

# Output index entries
for entry in indexentries:
    print entry["filename"]
    print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
    print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
    print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
    print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
    print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]

# Output TREE Extension data
for entry in extensiondata:
    print entry["sha1"] + " " + entry["path"] + " (" + entry["entry_count"] + " entries, " + entry["subtrees"] + " subtrees)"

sha1 = f.read(20)
print "SHA1 over the whole file: " + str(binascii.hexlify(sha1))
