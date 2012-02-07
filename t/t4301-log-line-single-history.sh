#!/bin/sh
#
# Copyright (c) 2010 Bo Yang
#

test_description='Test git log -L with single line of history'

. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

	cat >patch_series <<\EOF &&
From e6de34ec1fc29ba2d8d10d64dcb708196c6b4255 Mon Sep 17 00:00:00 2001
Message-Id: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Junio C Hamano <junkio@cox.net>
Date: Mon, 29 May 2006 22:13:09 -0700
Subject: [PATCH 01/10] Merge branch 'jc/lt-tree-n-cache-tree' into lt/tree-2

* jc/lt-tree-n-cache-tree:
  adjust to the rebased series by Linus.
  Remove "tree->entries" tree-entry list from tree parser
  Switch "read_tree_recursive()" over to tree-walk functionality
  Make "tree_entry" have a SHA1 instead of a union of object pointers
  Add raw tree buffer info to "struct tree"

This results as if an "ours" merge absorbed the previous "next"
branch change into the 10-patch series, but it really is a result
of an honest merge.

nothing to commit
---
 object.c |   40 ++++++++++++++++++++++++++++++++++++++++
 1 files changed, 40 insertions(+), 0 deletions(-)
 create mode 100644 object.c

diff --git a/object.c b/object.c
new file mode 100644
index 0000000..8f15b85
--- /dev/null
+++ b/object.c
@@ -0,0 +1,40 @@
+struct object *parse_object(const unsigned char *sha1)
+{
+	unsigned long size;
+	char type[20];
+	void *buffer = read_sha1_file(sha1, type, &size);
+	if (buffer) {
+		struct object *obj;
+		if (check_sha1_signature(sha1, buffer, size, type) < 0)
+			printf("sha1 mismatch %s\n", sha1_to_hex(sha1));
+		if (!strcmp(type, blob_type)) {
+			struct blob *blob = lookup_blob(sha1);
+			parse_blob_buffer(blob, buffer, size);
+			obj = &blob->object;
+		} else if (!strcmp(type, tree_type)) {
+			struct tree *tree = lookup_tree(sha1);
+			obj = &tree->object;
+			if (!tree->object.parsed) {
+				parse_tree_buffer(tree, buffer, size);
+				buffer = NULL;
+			}
+		} else if (!strcmp(type, commit_type)) {
+			struct commit *commit = lookup_commit(sha1);
+			parse_commit_buffer(commit, buffer, size);
+			if (!commit->buffer) {
+				commit->buffer = buffer;
+				buffer = NULL;
+			}
+			obj = &commit->object;
+		} else if (!strcmp(type, tag_type)) {
+			struct tag *tag = lookup_tag(sha1);
+			parse_tag_buffer(tag, buffer, size);
+			obj = &tag->object;
+		} else {
+			obj = NULL;
+		}
+		free(buffer);
+		return obj;
+	}
+	return NULL;
+}
-- 
1.7.4.rc2.316.g66007


From c0741c93c2c45563a2d7cd41ec006948adfc173d Mon Sep 17 00:00:00 2001
Message-Id: <c0741c93c2c45563a2d7cd41ec006948adfc173d.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Junio C Hamano <junkio@cox.net>
Date: Fri, 15 Sep 2006 13:30:02 -0700
Subject: [PATCH 02/10] Add git-for-each-ref: helper for language bindings

This adds a new command, git-for-each-ref.  You can have it iterate
over refs and have it output various aspects of the objects they
refer to.

Signed-off-by: Junio C Hamano <junkio@cox.net>
---
 object.c |   67 +++++++++++++++++++++++++++++++++++++-------------------------
 1 files changed, 40 insertions(+), 27 deletions(-)

diff --git a/object.c b/object.c
index 8f15b85..24595e2 100644
--- a/object.c
+++ b/object.c
@@ -1,39 +1,52 @@
+struct object *parse_object_buffer(const unsigned char *sha1, const char *type, unsigned long size, void *buffer, int *eaten_p)
+{
+	struct object *obj;
+	int eaten = 0;
+
+	if (!strcmp(type, blob_type)) {
+		struct blob *blob = lookup_blob(sha1);
+		parse_blob_buffer(blob, buffer, size);
+		obj = &blob->object;
+	} else if (!strcmp(type, tree_type)) {
+		struct tree *tree = lookup_tree(sha1);
+		obj = &tree->object;
+		if (!tree->object.parsed) {
+			parse_tree_buffer(tree, buffer, size);
+			eaten = 1;
+		}
+	} else if (!strcmp(type, commit_type)) {
+		struct commit *commit = lookup_commit(sha1);
+		parse_commit_buffer(commit, buffer, size);
+		if (!commit->buffer) {
+			commit->buffer = buffer;
+			eaten = 1;
+		}
+		obj = &commit->object;
+	} else if (!strcmp(type, tag_type)) {
+		struct tag *tag = lookup_tag(sha1);
+		parse_tag_buffer(tag, buffer, size);
+		obj = &tag->object;
+	} else {
+		obj = NULL;
+	}
+	*eaten_p = eaten;
+	return obj;
+}
 struct object *parse_object(const unsigned char *sha1)
 {
 	unsigned long size;
 	char type[20];
+	int eaten;
 	void *buffer = read_sha1_file(sha1, type, &size);
+
 	if (buffer) {
 		struct object *obj;
 		if (check_sha1_signature(sha1, buffer, size, type) < 0)
 			printf("sha1 mismatch %s\n", sha1_to_hex(sha1));
-		if (!strcmp(type, blob_type)) {
-			struct blob *blob = lookup_blob(sha1);
-			parse_blob_buffer(blob, buffer, size);
-			obj = &blob->object;
-		} else if (!strcmp(type, tree_type)) {
-			struct tree *tree = lookup_tree(sha1);
-			obj = &tree->object;
-			if (!tree->object.parsed) {
-				parse_tree_buffer(tree, buffer, size);
-				buffer = NULL;
-			}
-		} else if (!strcmp(type, commit_type)) {
-			struct commit *commit = lookup_commit(sha1);
-			parse_commit_buffer(commit, buffer, size);
-			if (!commit->buffer) {
-				commit->buffer = buffer;
-				buffer = NULL;
-			}
-			obj = &commit->object;
-		} else if (!strcmp(type, tag_type)) {
-			struct tag *tag = lookup_tag(sha1);
-			parse_tag_buffer(tag, buffer, size);
-			obj = &tag->object;
-		} else {
-			obj = NULL;
-		}
-		free(buffer);
+
+		obj = parse_object_buffer(sha1, type, size, buffer, &eaten);
+		if (!eaten)
+			free(buffer);
 		return obj;
 	}
 	return NULL;
-- 
1.7.4.rc2.316.g66007


From aa0cab1ffa13ed8bc5b5a6ebbac02a1b2f7e65b1 Mon Sep 17 00:00:00 2001
Message-Id: <aa0cab1ffa13ed8bc5b5a6ebbac02a1b2f7e65b1.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Nicolas Pitre <nico@cam.org>
Date: Mon, 26 Feb 2007 14:55:59 -0500
Subject: [PATCH 03/10] convert object type handling from a string to a number

We currently have two parallel notation for dealing with object types
in the code: a string and a numerical value.  One of them is obviously
redundent, and the most used one requires more stack space and a bunch
of strcmp() all over the place.

This is an initial step for the removal of the version using a char array
found in object reading code paths.  The patch is unfortunately large but
there is no sane way to split it in smaller parts without breaking the
system.

Signed-off-by: Nicolas Pitre <nico@cam.org>
Signed-off-by: Junio C Hamano <junkio@cox.net>
---
 object.c |   16 ++++++++--------
 1 files changed, 8 insertions(+), 8 deletions(-)

diff --git a/object.c b/object.c
index 24595e2..85bb28c 100644
--- a/object.c
+++ b/object.c
@@ -1,20 +1,20 @@
-struct object *parse_object_buffer(const unsigned char *sha1, const char *type, unsigned long size, void *buffer, int *eaten_p)
+struct object *parse_object_buffer(const unsigned char *sha1, enum object_type type, unsigned long size, void *buffer, int *eaten_p)
 {
 	struct object *obj;
 	int eaten = 0;
 
-	if (!strcmp(type, blob_type)) {
+	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
 		parse_blob_buffer(blob, buffer, size);
 		obj = &blob->object;
-	} else if (!strcmp(type, tree_type)) {
+	} else if (type == OBJ_TREE) {
 		struct tree *tree = lookup_tree(sha1);
 		obj = &tree->object;
 		if (!tree->object.parsed) {
 			parse_tree_buffer(tree, buffer, size);
 			eaten = 1;
 		}
-	} else if (!strcmp(type, commit_type)) {
+	} else if (type == OBJ_COMMIT) {
 		struct commit *commit = lookup_commit(sha1);
 		parse_commit_buffer(commit, buffer, size);
 		if (!commit->buffer) {
@@ -22,7 +22,7 @@ struct object *parse_object_buffer(const unsigned char *sha1, const char *type,
 			eaten = 1;
 		}
 		obj = &commit->object;
-	} else if (!strcmp(type, tag_type)) {
+	} else if (type == OBJ_TAG) {
 		struct tag *tag = lookup_tag(sha1);
 		parse_tag_buffer(tag, buffer, size);
 		obj = &tag->object;
@@ -35,13 +35,13 @@ struct object *parse_object_buffer(const unsigned char *sha1, const char *type,
 struct object *parse_object(const unsigned char *sha1)
 {
 	unsigned long size;
-	char type[20];
+	enum object_type type;
 	int eaten;
-	void *buffer = read_sha1_file(sha1, type, &size);
+	void *buffer = read_sha1_file(sha1, &type, &size);
 
 	if (buffer) {
 		struct object *obj;
-		if (check_sha1_signature(sha1, buffer, size, type) < 0)
+		if (check_sha1_signature(sha1, buffer, size, typename(type)) < 0)
 			printf("sha1 mismatch %s\n", sha1_to_hex(sha1));
 
 		obj = parse_object_buffer(sha1, type, size, buffer, &eaten);
-- 
1.7.4.rc2.316.g66007


From b734dcbb044b08802edd0215d328b4554e29ff23 Mon Sep 17 00:00:00 2001
Message-Id: <b734dcbb044b08802edd0215d328b4554e29ff23.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Linus Torvalds <torvalds@linux-foundation.org>
Date: Tue, 20 Mar 2007 10:05:20 -0700
Subject: [PATCH 04/10] Don't ever return corrupt objects from "parse_object()"

Looking at the SHA1 validation code due to the corruption that Alexander
Litvinov is seeing under Cygwin, I notice that one of the most central
places where we read objects, we actually do end up verifying the SHA1 of
the result, but then we happily parse it anyway.

And using "printf" to write the error message means that it not only can
get lost, but will actually mess up stdout, and cause other strange and
hard-to-debug failures downstream.

Signed-off-by: Linus Torvalds <torvalds@linux-foundation.org>
Signed-off-by: Junio C Hamano <junkio@cox.net>
---
 object.c |    6 ++++--
 1 files changed, 4 insertions(+), 2 deletions(-)

diff --git a/object.c b/object.c
index 85bb28c..062d453 100644
--- a/object.c
+++ b/object.c
@@ -41,8 +41,10 @@ struct object *parse_object(const unsigned char *sha1)
 
 	if (buffer) {
 		struct object *obj;
-		if (check_sha1_signature(sha1, buffer, size, typename(type)) < 0)
-			printf("sha1 mismatch %s\n", sha1_to_hex(sha1));
+		if (check_sha1_signature(sha1, buffer, size, typename(type)) < 0) {
+			error("sha1 mismatch %s\n", sha1_to_hex(sha1));
+			return NULL;
+		}
 
 		obj = parse_object_buffer(sha1, type, size, buffer, &eaten);
 		if (!eaten)
-- 
1.7.4.rc2.316.g66007


From 84929bf3c54efc0c8980b1df5f37b213c3fe315c Mon Sep 17 00:00:00 2001
Message-Id: <84929bf3c54efc0c8980b1df5f37b213c3fe315c.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Carlos Rica <jasampler@gmail.com>
Date: Fri, 25 May 2007 03:46:22 +0200
Subject: [PATCH 05/10] fix memory leak in parse_object when check_sha1_signature fails

When check_sha1_signature fails, program is not terminated:
it prints an error message and returns NULL, so the
buffer returned by read_sha1_file should be freed before.

Signed-off-by: Carlos Rica <jasampler@gmail.com>
Signed-off-by: Junio C Hamano <junkio@cox.net>
---
 object.c |    1 +
 1 files changed, 1 insertions(+), 0 deletions(-)

diff --git a/object.c b/object.c
index 062d453..81e4fd6 100644
--- a/object.c
+++ b/object.c
@@ -42,6 +42,7 @@ struct object *parse_object(const unsigned char *sha1)
 	if (buffer) {
 		struct object *obj;
 		if (check_sha1_signature(sha1, buffer, size, typename(type)) < 0) {
+			free(buffer);
 			error("sha1 mismatch %s\n", sha1_to_hex(sha1));
 			return NULL;
 		}
-- 
1.7.4.rc2.316.g66007


From c3755af123ed46a97377232d4a1bcc5bf5dbf478 Mon Sep 17 00:00:00 2001
Message-Id: <c3755af123ed46a97377232d4a1bcc5bf5dbf478.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Sam Vilain <sam.vilain@catalyst.net.nz>
Date: Wed, 6 Jun 2007 22:25:17 +1200
Subject: [PATCH 06/10] Don't assume tree entries that are not dirs are blobs

When scanning the trees in track_tree_refs() there is a "lazy" test
that assumes that entries are either directories or files.  Don't do
that.

Signed-off-by: Junio C Hamano <gitster@pobox.com>
---
 object.c |    3 +++
 1 files changed, 3 insertions(+), 0 deletions(-)

diff --git a/object.c b/object.c
index 81e4fd6..35272ca 100644
--- a/object.c
+++ b/object.c
@@ -27,8 +27,11 @@ struct object *parse_object_buffer(const unsigned char *sha1, enum object_type t
 		parse_tag_buffer(tag, buffer, size);
 		obj = &tag->object;
 	} else {
+		warning("object %s has unknown type id %d\n", sha1_to_hex(sha1), type);
 		obj = NULL;
 	}
+	if (obj && obj->type == OBJ_NONE)
+		obj->type = type;
 	*eaten_p = eaten;
 	return obj;
 }
-- 
1.7.4.rc2.316.g66007


From ac89501f27d57372187f259f0900201d2065f888 Mon Sep 17 00:00:00 2001
Message-Id: <ac89501f27d57372187f259f0900201d2065f888.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Jim Meyering <meyering@redhat.com>
Date: Fri, 21 Dec 2007 11:56:32 +0100
Subject: [PATCH 07/10] Don't dereference NULL upon lookup failure.

Instead, signal the error just like the case we do upon encountering
an object with an unknown type.

Signed-off-by: Jim Meyering <meyering@redhat.com>
Signed-off-by: Junio C Hamano <gitster@pobox.com>
---
 object.c |   35 ++++++++++++++++++++++-------------
 1 files changed, 22 insertions(+), 13 deletions(-)

diff --git a/object.c b/object.c
index 35272ca..d70ce7d 100644
--- a/object.c
+++ b/object.c
@@ -3,29 +3,38 @@ struct object *parse_object_buffer(const unsigned char *sha1, enum object_type t
 	struct object *obj;
 	int eaten = 0;
 
+	obj = NULL;
 	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
-		parse_blob_buffer(blob, buffer, size);
-		obj = &blob->object;
+		if (blob) {
+			parse_blob_buffer(blob, buffer, size);
+			obj = &blob->object;
+		}
 	} else if (type == OBJ_TREE) {
 		struct tree *tree = lookup_tree(sha1);
-		obj = &tree->object;
-		if (!tree->object.parsed) {
-			parse_tree_buffer(tree, buffer, size);
-			eaten = 1;
+		if (tree) {
+			obj = &tree->object;
+			if (!tree->object.parsed) {
+				parse_tree_buffer(tree, buffer, size);
+				eaten = 1;
+			}
 		}
 	} else if (type == OBJ_COMMIT) {
 		struct commit *commit = lookup_commit(sha1);
-		parse_commit_buffer(commit, buffer, size);
-		if (!commit->buffer) {
-			commit->buffer = buffer;
-			eaten = 1;
+		if (commit) {
+			parse_commit_buffer(commit, buffer, size);
+			if (!commit->buffer) {
+				commit->buffer = buffer;
+				eaten = 1;
+			}
+			obj = &commit->object;
 		}
-		obj = &commit->object;
 	} else if (type == OBJ_TAG) {
 		struct tag *tag = lookup_tag(sha1);
-		parse_tag_buffer(tag, buffer, size);
-		obj = &tag->object;
+		if (tag) {
+			parse_tag_buffer(tag, buffer, size);
+			obj = &tag->object;
+		}
 	} else {
 		warning("object %s has unknown type id %d\n", sha1_to_hex(sha1), type);
 		obj = NULL;
-- 
1.7.4.rc2.316.g66007


From ff46c32a3753d46cc3c25ea2f0c1301747954e87 Mon Sep 17 00:00:00 2001
Message-Id: <ff46c32a3753d46cc3c25ea2f0c1301747954e87.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Martin Koegler <mkoegler@auto.tuwien.ac.at>
Date: Sun, 3 Feb 2008 22:22:39 +0100
Subject: [PATCH 08/10] parse_object_buffer: don't ignore errors from the object specific parsing functions

In the case of an malformed object, the object specific parsing functions
would return an error, which is currently ignored. The object can be partial
initialized in this case.

This patch make parse_object_buffer propagate such errors.

Signed-off-by: Martin Koegler <mkoegler@auto.tuwien.ac.at>
Signed-off-by: Junio C Hamano <gitster@pobox.com>
---
 object.c |   12 ++++++++----
 1 files changed, 8 insertions(+), 4 deletions(-)

diff --git a/object.c b/object.c
index d70ce7d..b748b60 100644
--- a/object.c
+++ b/object.c
@@ -7,7 +7,8 @@ struct object *parse_object_buffer(const unsigned char *sha1, enum object_type t
 	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
 		if (blob) {
-			parse_blob_buffer(blob, buffer, size);
+			if (parse_blob_buffer(blob, buffer, size))
+				return NULL;
 			obj = &blob->object;
 		}
 	} else if (type == OBJ_TREE) {
@@ -15,14 +16,16 @@ struct object *parse_object_buffer(const unsigned char *sha1, enum object_type t
 		if (tree) {
 			obj = &tree->object;
 			if (!tree->object.parsed) {
-				parse_tree_buffer(tree, buffer, size);
+				if (parse_tree_buffer(tree, buffer, size))
+					return NULL;
 				eaten = 1;
 			}
 		}
 	} else if (type == OBJ_COMMIT) {
 		struct commit *commit = lookup_commit(sha1);
 		if (commit) {
-			parse_commit_buffer(commit, buffer, size);
+			if (parse_commit_buffer(commit, buffer, size))
+				return NULL;
 			if (!commit->buffer) {
 				commit->buffer = buffer;
 				eaten = 1;
@@ -32,7 +35,8 @@ struct object *parse_object_buffer(const unsigned char *sha1, enum object_type t
 	} else if (type == OBJ_TAG) {
 		struct tag *tag = lookup_tag(sha1);
 		if (tag) {
-			parse_tag_buffer(tag, buffer, size);
+			if (parse_tag_buffer(tag, buffer, size))
+			       return NULL;
 			obj = &tag->object;
 		}
 	} else {
-- 
1.7.4.rc2.316.g66007


From a6b63c0b8b9fc595892c891e03f32b6b0fc03490 Mon Sep 17 00:00:00 2001
Message-Id: <a6b63c0b8b9fc595892c891e03f32b6b0fc03490.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: Christian Couder <chriscool@tuxfamily.org>
Date: Fri, 23 Jan 2009 10:07:10 +0100
Subject: [PATCH 09/10] object: call "check_sha1_signature" with the replacement sha1

Otherwise we get a "sha1 mismatch" error for replaced objects.

Signed-off-by: Christian Couder <chriscool@tuxfamily.org>
Signed-off-by: Junio C Hamano <gitster@pobox.com>
---
 object.c |    9 +++++----
 1 files changed, 5 insertions(+), 4 deletions(-)

diff --git a/object.c b/object.c
index b748b60..7524cc6 100644
--- a/object.c
+++ b/object.c
@@ -53,17 +53,18 @@ struct object *parse_object(const unsigned char *sha1)
 	unsigned long size;
 	enum object_type type;
 	int eaten;
-	void *buffer = read_sha1_file(sha1, &type, &size);
+	const unsigned char *repl;
+	void *buffer = read_sha1_file_repl(sha1, &type, &size, &repl);
 
 	if (buffer) {
 		struct object *obj;
-		if (check_sha1_signature(sha1, buffer, size, typename(type)) < 0) {
+		if (check_sha1_signature(repl, buffer, size, typename(type)) < 0) {
 			free(buffer);
-			error("sha1 mismatch %s\n", sha1_to_hex(sha1));
+			error("sha1 mismatch %s\n", sha1_to_hex(repl));
 			return NULL;
 		}
 
-		obj = parse_object_buffer(sha1, type, size, buffer, &eaten);
+		obj = parse_object_buffer(repl, type, size, buffer, &eaten);
 		if (!eaten)
 			free(buffer);
 		return obj;
-- 
1.7.4.rc2.316.g66007


From 28fac6c92f785c0c778db8e72d28e338d323e801 Mon Sep 17 00:00:00 2001
Message-Id: <28fac6c92f785c0c778db8e72d28e338d323e801.1295625079.git.trast@student.ethz.ch>
In-Reply-To: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
References: <e6de34ec1fc29ba2d8d10d64dcb708196c6b4255.1295625079.git.trast@student.ethz.ch>
From: =?UTF-8?q?Nguy=E1=BB=85n=20Th=C3=A1i=20Ng=E1=BB=8Dc=20Duy?= <pclouds@gmail.com>
Date: Fri, 3 Sep 2010 22:51:53 +0200
Subject: [PATCH 10/10] parse_object: pass on the original sha1, not the replaced one

Commit 0e87c36 (object: call "check_sha1_signature" with the
replacement sha1) changed the first argument passed to
parse_object_buffer() from "sha1" to "repl". With that change,
the returned obj pointer has the replacement SHA1 in obj->sha1,
not the original one.

But when using lookup_commit() and then parse_commit() on a
commit, we get an object pointer with the original sha1, but
the commit content comes from the replacement commit.

So the result we get from using parse_object() is different
from the we get from using lookup_commit() followed by
parse_commit().

It looks much simpler and safer to fix this inconsistency by
passing "sha1" to parse_object_bufer() instead of "repl".

The commit comment should be used to tell the the replacement
commit is replacing another commit and why. So it should be
easy to see that we have a replacement commit instead of an
original one.

And it is not a problem if the content of the commit is not
consistent with the sha1 as cat-file piped to hash-object can
be used to see the difference.

Signed-off-by: Christian Couder <chriscool@tuxfamily.org>
Signed-off-by: Junio C Hamano <gitster@pobox.com>
---
 object.c |    2 +-
 1 files changed, 1 insertions(+), 1 deletions(-)

diff --git a/object.c b/object.c
index 7524cc6..673ee0f 100644
--- a/object.c
+++ b/object.c
@@ -64,7 +64,7 @@ struct object *parse_object(const unsigned char *sha1)
 			return NULL;
 		}
 
-		obj = parse_object_buffer(repl, type, size, buffer, &eaten);
+		obj = parse_object_buffer(sha1, type, size, buffer, &eaten);
 		if (!eaten)
 			free(buffer);
 		return obj;
-- 
1.7.4.rc2.316.g66007
EOF

test_expect_success 'setup' '
	git am < patch_series
'

test_line_log_internal () {
	msg=$1 &&
	shift &&
	cat >expected &&
	test_expect_$mode "$msg" '
		git log --pretty=format:%s '"$*"'> actual &&
		test_cmp expected actual
	'
}

test_line_log () {
	mode=success &&
	test_line_log_internal "$@"
}

test_line_log_bug () {
	mode=failure &&
	test_line_log_internal "$@"
}

test_line_log 'Basic -L usage: line range (1)' -L3,10:object.c <<\EOF
parse_object_buffer: don't ignore errors from the object specific parsing functions
diff --git a/object.c b/object.c
index d70ce7d..b748b60 100644
--- a/object.c
+++ b/object.c
@@ -3,8 +3,8 @@
 	struct object *obj;
 	int eaten = 0;
 
 	obj = NULL;
 	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
 		if (blob) {
-			parse_blob_buffer(blob, buffer, size);
+			if (parse_blob_buffer(blob, buffer, size))

Don't dereference NULL upon lookup failure.
diff --git a/object.c b/object.c
index 35272ca..d70ce7d 100644
--- a/object.c
+++ b/object.c
@@ -3,7 +3,8 @@
 	struct object *obj;
 	int eaten = 0;
 
+	obj = NULL;
 	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
-		parse_blob_buffer(blob, buffer, size);
-		obj = &blob->object;
+		if (blob) {
+			parse_blob_buffer(blob, buffer, size);

convert object type handling from a string to a number
diff --git a/object.c b/object.c
index 24595e2..85bb28c 100644
--- a/object.c
+++ b/object.c
@@ -3,7 +3,7 @@
 	struct object *obj;
 	int eaten = 0;
 
-	if (!strcmp(type, blob_type)) {
+	if (type == OBJ_BLOB) {
 		struct blob *blob = lookup_blob(sha1);
 		parse_blob_buffer(blob, buffer, size);
 		obj = &blob->object;

Add git-for-each-ref: helper for language bindings
diff --git a/object.c b/object.c
index 8f15b85..24595e2 100644
--- a/object.c
+++ b/object.c
@@ -0,0 +3,7 @@
+	struct object *obj;
+	int eaten = 0;
+
+	if (!strcmp(type, blob_type)) {
+		struct blob *blob = lookup_blob(sha1);
+		parse_blob_buffer(blob, buffer, size);
+		obj = &blob->object;
EOF


test_line_log 'Basic -L usage: line range (2)' -L42,+3:object.c <<\EOF
Don't assume tree entries that are not dirs are blobs
diff --git a/object.c b/object.c
index 81e4fd6..35272ca 100644
--- a/object.c
+++ b/object.c
@@ -29,2 +29,3 @@
 	} else {
+		warning("object %s has unknown type id %d\n", sha1_to_hex(sha1), type);
 		obj = NULL;

Add git-for-each-ref: helper for language bindings
diff --git a/object.c b/object.c
index 8f15b85..24595e2 100644
--- a/object.c
+++ b/object.c
@@ -0,0 +29,2 @@
+	} else {
+		obj = NULL;
EOF


test_line_log_bug 'More than one line range' -L7,+3:object.c -L42,+3:object.c <<\EOF
Don't dereference NULL upon lookup failure.
diff --git a/object.c b/object.c
index 35272ca..d70ce7d 100644
--- a/object.c
+++ b/object.c
@@ -6,3 +7,3 @@
        if (type == OBJ_BLOB) {
                struct blob *blob = lookup_blob(sha1);
-               parse_blob_buffer(blob, buffer, size);
+               if (blob) {

convert object type handling from a string to a number
diff --git a/object.c b/object.c
index 24595e2..85bb28c 100644
--- a/object.c
+++ b/object.c
@@ -6,3 +6,3 @@
-       if (!strcmp(type, blob_type)) {
+       if (type == OBJ_BLOB) {
                struct blob *blob = lookup_blob(sha1);
                parse_blob_buffer(blob, buffer, size);
EOF

test_line_log 'Regex delimited range' -L/OBJ_TREE/,/}/:object.c <<\EOF
parse_object_buffer: don't ignore errors from the object specific parsing functions
diff --git a/object.c b/object.c
index d70ce7d..b748b60 100644
--- a/object.c
+++ b/object.c
@@ -13,8 +14,9 @@
 	} else if (type == OBJ_TREE) {
 		struct tree *tree = lookup_tree(sha1);
 		if (tree) {
 			obj = &tree->object;
 			if (!tree->object.parsed) {
-				parse_tree_buffer(tree, buffer, size);
+				if (parse_tree_buffer(tree, buffer, size))
+					return NULL;
 				eaten = 1;
 			}

Don't dereference NULL upon lookup failure.
diff --git a/object.c b/object.c
index 35272ca..d70ce7d 100644
--- a/object.c
+++ b/object.c
@@ -10,6 +13,8 @@
 	} else if (type == OBJ_TREE) {
 		struct tree *tree = lookup_tree(sha1);
-		obj = &tree->object;
-		if (!tree->object.parsed) {
-			parse_tree_buffer(tree, buffer, size);
-			eaten = 1;
+		if (tree) {
+			obj = &tree->object;
+			if (!tree->object.parsed) {
+				parse_tree_buffer(tree, buffer, size);
+				eaten = 1;
+			}

convert object type handling from a string to a number
diff --git a/object.c b/object.c
index 24595e2..85bb28c 100644
--- a/object.c
+++ b/object.c
@@ -10,6 +10,6 @@
-	} else if (!strcmp(type, tree_type)) {
+	} else if (type == OBJ_TREE) {
 		struct tree *tree = lookup_tree(sha1);
 		obj = &tree->object;
 		if (!tree->object.parsed) {
 			parse_tree_buffer(tree, buffer, size);
 			eaten = 1;

Add git-for-each-ref: helper for language bindings
diff --git a/object.c b/object.c
index 8f15b85..24595e2 100644
--- a/object.c
+++ b/object.c
@@ -0,0 +10,6 @@
+	} else if (!strcmp(type, tree_type)) {
+		struct tree *tree = lookup_tree(sha1);
+		obj = &tree->object;
+		if (!tree->object.parsed) {
+			parse_tree_buffer(tree, buffer, size);
+			eaten = 1;
EOF

test_expect_failure "Test more than one file" false

test_done
