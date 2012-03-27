#ifndef XNOTIFY_H
#define XNOTIFY_H

void xnotify_setup();
void xnotify_spawn_daemon();
int xnotify_path_unchanged(const char *path);

#endif /* XNOTIFY_H */
