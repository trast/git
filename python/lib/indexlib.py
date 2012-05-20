# Format strings
HEADER_FORMAT = """\
Signature: %(signature)s
Version: %(version)s
Number of entries: %(nrofentries)s"""

ENTRIES_FORMAT = """\
ctime: %(ctimesec)s:%(ctimensec)s
mtime: %(mtimesec)s:%(mtimensec)s
dev: %(dev)s\tino: %(ino)s
uid: %(uid)s\tgid: %(gid)s
size: %(filesize)s\tflags: """

EXTENSION_FORMAT = """\
%(sha1)s %(path)s (%(entry_count)s entries, %(subtrees)s subtrees)"""

REUCEXTENSION_FORMAT = """\
Path: %(path)s
Entrymode 1: %(entry_mode0)s Entrymode 2: %(entry_mode1)s Entrymode 3:\
        %(entry_mode2)s"""

EXTENSION_FORMAT_WITHOUT_SHA = """\
invalid %(path)s (%(entry_count)s entries, %(subtrees)s subtrees)"""
