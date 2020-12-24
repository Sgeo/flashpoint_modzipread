#pragma once

#include <zip.h>
#include <apr_pools.h>

zip_t * cached_zip_open(const char *path, int flags, int *errorp);
void initialize_zip_cache(apr_pool_t *pool);