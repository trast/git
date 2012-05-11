#!/usr/bin/env python

import hashlib
import binascii
import struct
import os.path
from collections import defaultdict

#  fread {{{
f = open(".git/index", "rb")
filedata = list()


def fread(n):
    global filedata
    data = f.read(n)
    filedata.append(data)
    return data
# }}}


# fwrite {{{
fw = open(".git/index-v5", "wb")
writtenbytes = 0
writtendata = list()


def fwrite(data):
    global writtenbytes
    global writtendata
    writtendata.append(data)
    writtenbytes += len(data)
    fw.write(data)
# }}}


# convert {{{
def convert(n):
    return str(struct.unpack('!I', n)[0])
# }}}


# readheader {{{
def readheader(f):
    # Signature
    signature = fread(4)
    header = struct.unpack('!II', fread(8))
    return dict({"signature": signature, "version": header[0], "nrofentries": header[1]})
# }}}


# readindexentries {{{
def readindexentries(f):
    indexentries = []
    conflictedentries = []
    paths = set()
    files = list()
    filedirs = defaultdict(list)
    byte = fread(1)
    i = 0
    # Read index entries
    while i < header["nrofentries"]:
        entry = struct.unpack('!IIIIIIIIII', byte + fread(39))  # stat data
        entry = entry + (str(binascii.hexlify(fread(20))),)     # SHA-1

        if (header["version"] == 3):
            entry = entry + struct.unpack('!hh', fread(4))      # Flags + extended flags
        else:
            entry = entry + struct.unpack('!h', fread(2))       # Flags

        string = ""
        byte = fread(1)
        readbytes = 1
        while byte != '\0':
            string = string + byte
            byte = fread(1)
            readbytes += 1

        pathname = os.path.dirname(string)
        filename = os.path.basename(string)
        paths.add(pathname)
        files.append(filename)
        filedirs[pathname].append(filename)

        entry = entry + (pathname, filename)           # Filename

        if (header["version"] == 3):
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec',
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'xtflags', 'pathname', 'filename'), entry))
        else:
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec',
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'pathname', 'filename'), entry))

        if header["version"] == 2:
            j = 8 - (readbytes + 5) % 8
        else:
            j = 8 - (readbytes + 1) % 8

        while byte == '\0' and j > 0:
            byte = fread(1)
            j -= 1

        stage = (entry[11] & 0b0011000000000000) / 0b001000000000000

        if stage == 0:      # Not conflicted
            indexentries.append(dictentry)
        else:                   # Conflicted
            if stage == 1:  # Write the stage 1 entry to the main index, to avoid rewriting the whole index once the conflict is resolved
                indexentries.append(dictentry)
            conflictedentries.append(dictentry)

        i = i + 1

    return indexentries, conflictedentries, byte, paths, files, filedirs
# }}}


# readextensiondata {{{
def readextensiondata(f):
    extensionsize = fread(4)

    read = 0
    subtreenr = [0]
    subtree = [""]
    listsize = 0
    extensiondata = dict()
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1

        while listsize >= 0 and subtreenr[listsize] == 0:
            subtreenr.pop()
            subtree.pop()
            listsize -= 1

        fpath = ""
        if listsize > 0:
            for p in subtree:
                if p != "":
                    fpath += p + "/"
            subtreenr[listsize] = subtreenr[listsize] - 1
        fpath += path + "/"

        entry_count = ""
        byte = fread(1)
        read += 1
        while byte != " ":
            entry_count += byte
            byte = fread(1)
            read += 1

        subtrees = ""
        byte = fread(1)
        read += 1
        while byte != "\n":
            subtrees += byte
            byte = fread(1)
            read += 1

        subtreenr.append(int(subtrees))
        subtree.append(path)
        listsize += 1

        if entry_count != "-1":
            sha1 = binascii.hexlify(fread(20))
            read += 20
        else:
            sha1 = "invalid"

        if sha1 == "invalid":
            extensiondata[fpath] = dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees})
        else:
            extensiondata[fpath] = dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees, "sha1": sha1})

    return extensiondata
# }}}


# readreucextensiondata {{{
def readreucextensiondata(f):
    extensionsize = fread(4)

    read = 0
    extensiondata = list()
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1

        entry_mode = list()
        i = 0
        while i < 3:
            byte = fread(1)
            read += 1
            mode = ""
            while byte != '\0':
                mode += byte
                byte = fread(1)
                read += 1
            i += 1

            entry_mode.append(int(mode, 8))

        i = 0
        obj_names = list()
        while i < 3:
            if entry_mode[i] != 0:
                obj_names.append(fread(20))
                read += 20
            else:
                obj_names.append("")
            i += 1

        extensiondata.append(dict({"path": path, "entry_mode0": entry_mode[0], "entry_mode1": entry_mode[1], "entry_mode2": entry_mode[2], "obj_names0": obj_names[0], "obj_names1": obj_names[1], "obj_names2": obj_names[2]}))

    return extensiondata

# }}}


# printheader {{{
def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["version"])
    print "Number of entries: " + str(header["nrofentries"])
# }}}


# printindexentries {{{
def printindexentries(indexentries):
    for entry in indexentries:
        if entry["pathname"] != "":
            print entry["pathname"] + "/" + entry["filename"]
        else:
            print entry["filename"]
        print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
        print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
        print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
        print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
        print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]
# }}}


# {{{ printextensiondata
def printextensiondata(extensiondata):
    for entry in extensiondata.viewvalues():
        print entry["sha1"] + " " + entry["path"] + " (" + entry["entry_count"] + " entries, " + entry["subtrees"] + " subtrees)"
# }}}


# printreucextensiondata {{{
def printreucextensiondata(extensiondata):
    for e in extensiondata:
        print "Path: " + e["path"]
        print "Entrymode 1: " + str(e["entry_mode0"]) + " Entrymode 2: " + str(e["entry_mode1"]) + " Entrymode 3: " + str(e["entry_mode2"])
        print "Objectnames 1: " + binascii.hexlify(e["obj_names0"]) + " Objectnames 2: " + binascii.hexlify(e["obj_names1"]) + " Objectnames 3: " + binascii.hexlify(e["obj_names2"])
# }}}


# Write stuff for index-v5 draft0 {{{
# writev5_0header {{{
def writev5_0header(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIIIQ", header["version"], len(paths), len(files), header["nrofentries"], 0))
# }}}


# writev5_0directories {{{
def writev5_0directories(paths, treeextensiondata):
    offsets = dict()
    subtreenr = dict()
    # Calculate subtree numbers
    for p in sorted(paths, reverse=True):
        splited = p.split("/")
        if p not in subtreenr:
            subtreenr[p] = 0
        if len(splited) > 1:
            i = 0
            path = ""
            while i < len(splited) - 1:
                path += "/" + splited[i]
                i += 1
            if path[1:] not in subtreenr:
                subtreenr[path[1:]] = 1
            else:
                subtreenr[path[1:]] += 1

    for p in paths:
        offsets[p] = writtenbytes
        fwrite(struct.pack("!Q", 0))
        fwrite(p.split("/")[-1] + "\0")
        p += "/"
        if p in treeextensiondata:
            fwrite(struct.pack("!ll", int(treeextensiondata[p]["entry_count"]), int(treeextensiondata[p]["subtrees"])))
            if (treeextensiondata[p]["entry_count"] != "-1"):
                fwrite(binascii.unhexlify(treeextensiondata[p]["sha1"]))

        else:  # If there is no cache-tree data we assume the entry is invalid
            fwrite(struct.pack("!ii", -1, subtreenr[p.strip("/")]))
    return offsets
# }}}


# writev5_0files {{{
def writev5_0files(files):
    offsets = dict()
    for f in files:
        offsets[f] = writtenbytes
        fwrite(f)
        fwrite("\0")
    return offsets
# }}}


# writev5_0fileentries {{{
def writev5_0fileentries(entries, fileoffsets):
    offsets = dict()
    for e in sorted(entries, key=lambda k: k['pathname']):
        if e["pathname"] not in offsets:
            offsets[e["pathname"]] = writtenbytes
        fwrite(struct.pack("!IIIIIIIIII", e["ctimesec"], e["ctimensec"],
            e["mtimesec"], e["mtimensec"], e["dev"], e["ino"], e["mode"],
            e["uid"], e["gid"], e["filesize"]))
        fwrite(binascii.unhexlify(e["sha1"]))

        try:
            fwrite(struct.pack("!III", e["flags"], e["xtflags"],
                fileoffsets[e["filename"]]))
        except KeyError:
            fwrite(struct.pack("!III", e["flags"], 0,
                fileoffsets[e["filename"]]))

        writecrc32()
    return offsets
# }}}


# writev5_0fileoffsets {{{
def writev5_0fileoffsets(diroffsets, fileoffsets, dircrcoffset):
    for d in sorted(diroffsets):
        fw.seek(diroffsets[d])
        fw.write(struct.pack("!Q", fileoffsets[d]))

    # Calculate crc32
    #f.seek(0)
    #data = f.read(dircrcoffset)
    fw.seek(dircrcoffset)
    fw.write(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))

# }}}


# writev5_0reucextensiondata {{{
def writev5_0reucextensiondata(data):
    global writtenbytes
    offset = writtenbytes
    for d in data:
        fwrite(d["path"])
        fwrite("\0")
        stages = set()
        fwrite(struct.pack("!b", 0))
        for i in xrange(0, 2):
            fwrite(struct.pack("!i", d["entry_mode" + str(i)]))
            if d["entry_mode" + str(i)] != 0:
                stages.add(i)

        for i in sorted(stages):
            fwrite(d["obj_names" + str(i)])
    writecrc32()
    fw.seek(20)
    fw.write(struct.pack("!Q", offset))
# }}}


# writev5_0conflicteddata {{{
def writev5_0conflicteddata(conflicteddata):
    print "Not implemented yet"
# }}}

# }}}


# Write stuff for index-v5 draft1 {{{
# Write header {{{
def writev5_1header(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIII", header["version"], len(paths), len(files), 0))
# }}}


# Write fake directory offsets which can only be filled in later {{{
def writev5_1fakediroffsets(paths):
    for p in paths:
        fwrite(struct.pack("!I", 0))
# }}}


# Write directories {{{
def writev5_1directories(paths):
    diroffsets = list()
    dirwritedataoffsets = dict()
    dirdata = defaultdict(dict)
    for p in sorted(paths):
        diroffsets.append(writtenbytes)

        # pathname
        if p == "":
            fwrite("\0")
        else:
            fwrite(p + "/\0")

        dirwritedataoffsets[p] = writtenbytes

        # flags, foffset, cr, ncr, nsubtrees, nfiles, nentries, objname, dircrc
        # All this fields will be filled out when the rest of the index
        # is written
        fwrite(struct.pack("!HIIIIIIIIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))

        # Subtreenr for later usage
        if p != "":
            path = p.split("/")
            try:
                dirdata["/".join(path[:-1])]["nsubtrees"] += 1
            except KeyError:
                dirdata["/".join(path[:-1])]["nsubtrees"] = 1

    return diroffsets, dirwritedataoffsets, dirdata
# }}}


# Write fake file offsets {{{
def writev5_1fakefileoffsets(indexentries):
    beginning = writtenbytes
    for f in indexentries:
        fwrite(struct.pack("!I", 0))
    return beginning
# }}}


# Write directory offsets for real {{{
def writev5_1diroffsets(offsets):
    fw.seek(24)
    for o in offsets:
        fw.write(struct.pack("!I", o))
# }}}


# Write file data {{{
def writev5_1filedata(indexentries, dirdata):
    global writtenbytes
    fileoffsets = list()
    for entry in sorted(indexentries, key=lambda k: k['pathname']):
        offset = writtenbytes
        fileoffsets.append(offset)
        fwrite(entry["filename"] + "\0")

        # Prepare flags
        # TODO: Consider extended flags
        flags = entry["flags"] & 0b1000000000000000
        flags += (entry["flags"] & 0b0011000000000000) * 2
        fwrite(struct.pack("!I", flags))

        # mode
        fwrite(struct.pack("!I", entry["mode"]))

        # mtime
        fwrite(struct.pack("!I", entry["mtimesec"]))
        fwrite(struct.pack("!I", entry["mtimensec"]))

        # calculate crc for stat data
        crc = binascii.crc32(struct.pack("!IIIIIIII", offset, entry["ctimesec"], entry["ctimensec"], entry["ino"], entry["filesize"], entry["dev"], entry["uid"], entry["gid"]))
        fwrite(struct.pack("!i", crc))

        fwrite(binascii.unhexlify(entry["sha1"]))

        writecrc32()
        try:
            dirdata[entry["pathname"]]["nfiles"] += 1
        except KeyError:
            dirdata[entry["pathname"]]["nfiles"] = 1

    return fileoffsets, dirdata

# }}}


# Write file offsets for read {{{
def writev5_1fileoffsets(foffsets, fileoffsetbeginning):
    fw.seek(fileoffsetbeginning)
    for f in foffsets:
        fw.write(struct.pack("!I", f))
# }}}


# Write correct directory data {{{
def writev5_1directorydata(dirdata, dirwritedataoffsets, fileoffsetbeginning):
    global writtendata
    foffset = fileoffsetbeginning
    for d in sorted(dirdata.iteritems()):
        try:
            fw.seek(dirwritedataoffsets[d[0]])
        except KeyError:
            continue
        writtendata = [d[0] + "\0"]
        try:
            nsubtrees = d[1]["nsubtrees"]
        except KeyError:
            nsubtrees = 0

        try:
            nfiles = d[1]["nfiles"]
        except KeyError:
            nfiles = 0

        try:
            fwrite(struct.pack("!H", d[1]["flags"]))
        except KeyError:
            fwrite(struct.pack("!H", 0))

        if nfiles == -1 or nfiles == 0:
            fwrite(struct.pack("!I", 0))
        else:
            fwrite(struct.pack("!I", foffset))
            foffset += (nfiles) * 4

        try:
            fwrite(struct.pack("!I", d[1]["cr"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        try:
            fwrite(struct.pack("!I", d[1]["ncr"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        fwrite(struct.pack("!I", nsubtrees))
        fwrite(struct.pack("!I", nfiles))

        try:
            fwrite(struct.pack("!i", d[1]["nentries"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        try:
            fwrite(binascii.unhexlify(d[1]["objname"]))
        except KeyError:
            fwrite(struct.pack("!IIIII", 0, 0, 0, 0, 0))

        writecrc32()
# }}}


# Write conflicted data {{{
def writev5_1conflicteddata(conflictedentries, reucdata, dirdata):
    global writtenbytes
    for d in sorted(conflictedentries):
        if d["pathname"] == "":
            filename = d["filename"]
        else:
            filename = d["pathname"] + "/" + d["filename"]

        dirdata[filename]["cr"] = fw.tell()
        try:
            dirdata[filename]["ncr"] += 1
        except KeyError:
            dirdata[filename]["ncr"] = 1

        fwrite(d["pathname"] + d["filename"])
        fwrite("\0")
        stages = set()
        fwrite(struct.pack("!b", 0))
        for i in xrange(0, 2):
            fwrite(struct.pack("!i", d["mode"]))
            if d["mode"] != 0:
                stages.add(i)

        for i in sorted(stages):
            print i
            fwrite(binascii.unhexlify(d["sha1"]))

        writecrc32()

    return dirdata
# }}}


# Compile cachetreedata and factor it into the dirdata
def compilev5_1cachetreedata(dirdata, extensiondata):
    for entry in extensiondata.iteritems():
        dirdata[entry[1]["path"].strip("/")]["nentries"] = int(entry[1]["entry_count"])
        try:
            dirdata[entry[1]["path"].strip("/")]["objname"] = entry[1]["sha1"]
        except:
            pass  # Cache tree entry invalid

        try:
            if dirdata[entry[1]["path"].strip("/")]["nsubtrees"] != entry[1]["subtreenr"]:
                print entry[0]
                print dirdata[entry[1]["path"].strip("/")]["nsubtrees"]
                print entry[1]["subtreenr"]
        except KeyError:
            pass

    return dirdata
# }}}

# }}}


# writecrc32 {{{
def writecrc32():
    global writtendata
    crc = binascii.crc32("".join(writtendata))
    fwrite(struct.pack("!i", crc))
    writtendata = list()  # Reset writtendata for next crc32
# }}}


header = readheader(f)

indexentries, conflictedentries, byte, paths, files, filedirs = readindexentries(f)

filedata = filedata[:-1]
ext = byte + f.read(3)
extensiondata = []

ext2 = ""
if ext == "TREE":
    filedata += ext
    treeextensiondata = readextensiondata(f)
    ext2 = f.read(4)
else:
    treeextensiondata = dict()

if ext == "REUC" or ext2 == "REUC":
    if ext == "REUC":
        filedata += ext
    else:
        filedata += ext2
    reucextensiondata = readreucextensiondata(f)
else:
    reucextensiondata = list()

# printheader(header)
# printindexentries(indexentries)
# printextensiondata(extensiondata)
# printreucextensiondata(reucextensiondata)


if ext != "TREE" and ext != "REUC" and ext2 != "REUC":
    sha1read = ext + f.read(16)
elif ext2 != "REUC" and ext == "TREE":
    sha1read = ext2 + f.read(16)
else:
    sha1read = f.read(20)

print "SHA1 over the whole file: " + str(binascii.hexlify(sha1read))

sha1 = hashlib.sha1()
sha1.update("".join(filedata))
print "SHA1 over filedata: " + str(sha1.hexdigest())

if sha1.hexdigest() == binascii.hexlify(sha1read):
    # Write v5_0 {{{
    # writev5_0header(header, paths, files)
    # diroffsets = writev5_0directories(sorted(paths), treeextensiondata)
    # fileoffsets = writev5_0files(files)
    # dircrcoffset = writtenbytes

    # # TODO: Replace with something faster. Doesn't make sense to calculate crc32 here
    # writecrc32()
    # fileoffsets = writev5_0fileentries(indexentries, fileoffsets)
    # writev5_0reucextensiondata(reucextensiondata)
    # writev5_0conflicteddata(conflictedentries)
    # writev5_0fileoffsets(diroffsets, fileoffsets, dircrcoffset)
    # }}}

    # Write v5_1 {{{
    writev5_1header(header, paths, files)
    writecrc32()
    writev5_1fakediroffsets(paths)
    # writecrc32() # TODO Check if needed
    diroffsets, dirwritedataoffsets, dirdata = writev5_1directories(paths)
    fileoffsetbeginning = writev5_1fakefileoffsets(indexentries)
    # writecrc32() # TODO Check if needed
    fileoffsets, dirdata = writev5_1filedata(indexentries, dirdata)
    dirdata = writev5_1conflicteddata(conflictedentries, reucextensiondata, dirdata)
    writev5_1diroffsets(diroffsets)
    writev5_1fileoffsets(fileoffsets, fileoffsetbeginning)
    dirdata = compilev5_1cachetreedata(dirdata, treeextensiondata)
    writev5_1directorydata(dirdata, dirwritedataoffsets, fileoffsetbeginning)
    # }}}
else:
    print "File is corrupted"
