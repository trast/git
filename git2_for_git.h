#ifndef GIT2_FOR_GIT_H
#define GIT2_FOR_GIT_H

#define git_config git2_config
#define git_config_set_multivar git2_config_set_multivar

#include <git2.h>

#undef git_config
#undef git_config_set_multivar

#endif /* GIT2_FOR_GIT_H */
