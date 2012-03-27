#include "cache.h"
#include "strbuf.h"
#include "string-list.h"
#include <assert.h>

/* FIXME */
#define XNOTIFY_INOTIFY

#ifdef XNOTIFY_INOTIFY
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <sys/time.h>

char *xnotify_socket;
struct sockaddr_un addr, peer;
int xnotify_fd = -1;
struct string_list known_changed = STRING_LIST_INIT_DUP; /* FIXME use hash */

static void xnotify_init_socket()
{
	struct strbuf sb = STRBUF_INIT;

	if (xnotify_fd >= 0)
		return;

	strbuf_addstr(&sb, get_index_file());
	strbuf_addstr(&sb, "-xnotify.socket");
	xnotify_socket = strbuf_detach(&sb, NULL);
	xnotify_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (xnotify_socket < 0)
		die_errno("cannot create xnotify socket");
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, xnotify_socket, sizeof(addr.sun_path)-1);
}

void xnotify_setup()
{
	ssize_t len, nread;
	char buf[8192], *p;
	char *env;

	env = getenv("GIT_XNOTIFY");
	if (env && !*env)
		return;

	if (core_xnotify_daemon != -1)
		return;

	xnotify_init_socket();

	if (connect(xnotify_fd, &addr, sizeof(struct sockaddr_un))) {
		if (errno != ECONNREFUSED && errno != ENOENT)
			die_errno("cannot connect to xnotify socket");
		/* no daemon yet; we might still start one later */
		unlink(xnotify_socket);
		core_xnotify_daemon = 1;
		return;
	}

	write_in_full(xnotify_fd, "changed\n", 8);

	p = buf;
	while ((nread = xread(xnotify_fd, p, sizeof(buf)-(p-buf)))) {
		char *begin, *end;
		if (nread < 0)
			die_errno("read from xnotify");
		len = nread + (p - buf);
		begin = buf;
		while ((end = memchr(begin, '\n', len-(begin-buf)))) {
			if (end == begin)
				goto done;
			*end = '\0';
			string_list_append(&known_changed, begin);
			begin = end + 1;
		}
		memmove(buf, begin, len-(begin-buf));
		p = buf + len-(begin-buf);
	}

done:
	write_in_full(xnotify_fd, "done\n", 5);

	if (close(xnotify_fd))
		die_errno("cannot close xnotify socket");

	sort_string_list(&known_changed);
	/* found a daemon; do not start another */
	core_xnotify_daemon = 2;
}

char **wdpaths;
int wd_alloc;

static char *set_dirpath(int wd, char *path)
{
	int old_alloc = wd_alloc;
	ALLOC_GROW(wdpaths, wd+1, wd_alloc);
	if (old_alloc < wd_alloc)
		memset(wdpaths+old_alloc, 0,
		       (wd_alloc-old_alloc)*sizeof(const char *));
	wdpaths[wd] = strdup(path);
	return wdpaths[wd];
}

const int INOTIFY_MASK =
	IN_CREATE
	| IN_MODIFY
	| IN_MOVE_SELF
	| IN_MOVED_TO
	| IN_DONT_FOLLOW
	| IN_EXCL_UNLINK;

static void handle_event(struct inotify_event *ev)
{
	char *wdp;
	char buf[PATH_MAX];

	/* If there was a queue overflow, we have to assume the worst */
	if (ev->mask & IN_Q_OVERFLOW)
		exit(0);
	if (ev->wd < 0)
		return;
	wdp = wdpaths[ev->wd];
	if (!wdp)
		return;

	buf[0] = '\0';
	if (strcmp(wdp, ".")) {
		strcpy(buf, wdp);
		strcat(buf, "/");
	}
	strcat(buf, ev->name);

	string_list_append(&known_changed, buf);
}

static inline int event_len(struct inotify_event *ev)
{
	return sizeof(struct inotify_event) + ev->len;
}

static void handle_inotify (int ifd)
{
	char buf[4096+PATH_MAX];
	ssize_t ret = read(ifd, &buf, sizeof(buf));
	int handled = 0;
	if (ret == -1) {
		if (errno == EINTR)
			return;
		die_errno("read");
	}
	while (handled < ret) {
		char *p = buf + handled;
		struct inotify_event *ev = (struct inotify_event *) p;
		handle_event(ev);
		handled += event_len(ev);
	}
}

static void send_changed (int conn)
{
	char buf[8192];
	char *p = buf;
	char *end = buf + sizeof(buf);
	struct string_list_item *item;

	for_each_string_list_item(item, &known_changed) {
		int len = strlen(item->string);
		if (p+len >= end) {
			write_in_full(conn, buf, p-buf);
			p = buf;
		}
		memcpy(p, item->string, len);
		p += len;
		*p++ = '\n';
	}

	write_in_full(conn, buf, p-buf);
	write_in_full(conn, "\n", 1);
}

static void handle_conn (int conn)
{
	struct strbuf sb = STRBUF_INIT;

	while (strbuf_getwholeline_fd(&sb, conn, '\n') != EOF) {
		if (!strcmp("changed\n", sb.buf))
			send_changed(conn);
		else if (!strcmp("done\n", sb.buf))
			break;
		else {
			strbuf_setlen(&sb, sb.len-1);
			die("unknown xnotify command: '%s'", sb.buf);
		}
	}

	if (close(conn))
		die_errno("close");
}

static void xnotify_child()
{
	int maxfd;
	int ifd;
	char *prev_dir = "";

	int i;

	ifd = inotify_init();
	if (ifd < 0)
		die_errno("inotify_init");

	for (i = 0; i < active_nr; i++){
		char buf[PATH_MAX+1];
		char *slash;
		int wd;
		struct cache_entry *ce = active_cache[i];
		slash = strrchr(ce->name, '/');
		if (slash) {
			memcpy(buf, ce->name, slash-ce->name);
			buf[slash-ce->name] = '\0';
		} else {
			strcpy(buf, ".");
		}
		if (strcmp(prev_dir, buf)) {
			wd = inotify_add_watch(ifd, buf, INOTIFY_MASK);
			prev_dir = set_dirpath(wd, buf);
		}
		if (!ce_uptodate(ce))
			string_list_append(&known_changed, ce->name);
	}

	xnotify_init_socket();

	if (bind(xnotify_fd, (struct sockaddr *) &addr, sizeof(addr)))
		die_errno("cannot bind xnotify socket");
	if (listen(xnotify_fd, 10))
		die_errno("cannot listen on xnotify socket");

	maxfd = ifd;
	if (maxfd < xnotify_fd)
		maxfd = xnotify_fd;

	while (1) {
		fd_set fds;
		int ret;
		int conn;
		socklen_t peer_sz;
		struct timeval timeout = {60, 0};

		FD_ZERO(&fds);
		FD_SET(ifd, &fds);
		FD_SET(xnotify_fd, &fds);
		ret = select(maxfd+1, &fds, NULL, NULL, &timeout);
		if (ret == -1)
			die_errno("select");
		if (!ret)
			break;
		if (FD_ISSET(ifd, &fds))
			handle_inotify(ifd);
		if (FD_ISSET(xnotify_fd, &fds)) {
			conn = accept(xnotify_fd, (struct sockaddr *) &peer, &peer_sz);
			if (conn < 0)
				die_errno("accept");
			handle_conn(conn);
			close(conn); /* errors ignored */
		}
	}
}

void xnotify_spawn_daemon()
{
	if (core_xnotify_daemon != 1)
		return;

	if (fork())
		return;

	xnotify_child();
}

int xnotify_path_unchanged(const char *path)
{
	if (core_xnotify_daemon != 2)
		return 0;
	return !string_list_has_string(&known_changed, path);
}

#else

void xnotify_setup()
{
	core_xnotify_daemon = 0;
}

void xnotify_spawn_daemon()
{
}

int xnotify_path_unchanged(const char *path)
{
	return 0;
}

#endif
