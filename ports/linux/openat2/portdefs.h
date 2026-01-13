/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */
#if __has_include (<linux/openat2.h>)
# include <linux/openat2.h>
#else
  struct open_how {
        __u64 flags;
        __u64 mode;
        __u64 resolve;
  };
#endif
