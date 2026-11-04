/* SPDX-License-Identifier: Apache-2.0
 * Included by lfs_vfs.c. The mount mutex protects every node and page list.
 */
#define LITTLEFS_PAGE_SIZE 4096
#define LITTLEFS_MAX_PAGES 64
#ifdef CONFIG_FS_LITTLEFS_COALESCE
void *coalesce_page_alloc(void);
int coalesce_page_free(void *buffer);
#endif

static int littlefs_node_error(FAR struct littlefs_node_s *node, int ret)
{
  if (ret < 0 && node->error == 0)
    {
      node->error = ret;
    }

  return ret;
}

static bool littlefs_unlinked(FAR struct littlefs_node_s *node)
{
  return node->file.m.pair[0] == (lfs_block_t)-1;
}

static int littlefs_lookup(FAR struct littlefs_mountpt_s *fs,
                           FAR const char *path,
                           FAR struct littlefs_node_s **found)
{
  lfs_block_t pair[2];
  uint16_t id;
  int ret;
  *found = NULL;
  ret = littlefs_convert_result(lfs_file_identity(&fs->lfs, path, pair, &id));
  if (ret < 0)
    {
      return ret;
    }

  for (FAR struct littlefs_node_s *n = fs->nodes; n; n = n->next)
    {
      if (!littlefs_unlinked(n) && n->file.id == id &&
          (n->file.m.pair[0] == pair[0] || n->file.m.pair[0] == pair[1] ||
           n->file.m.pair[1] == pair[0] || n->file.m.pair[1] == pair[1]))
        {
          *found = n;
          break;
        }
    }

  return 0;
}

static void littlefs_drop_pages(FAR struct littlefs_node_s *node)
{
  while (node->pages)
    {
      FAR struct littlefs_page_s *p = node->pages;
      node->pages = p->next;
#ifdef CONFIG_FS_LITTLEFS_COALESCE
      int ret = coalesce_page_free(p->data);
      DEBUGASSERT(ret == 0);
      UNUSED(ret);
#endif
      fs_heap_free(p);
    }
}

static int littlefs_node_sync(FAR struct littlefs_mountpt_s *fs,
                              FAR struct littlefs_node_s *node)
{
  int ret;
  if (node->error)
    {
      return node->error;
    }

  if (node->file.flags & LFS_F_ERRED)
    {
      return littlefs_node_error(node, -EIO);
    }

#ifdef CONFIG_FS_LITTLEFS_COALESCE
  if (node->pages)
    {
      FAR struct lfs_update_range *ranges = fs->updates;
      unsigned int count = 0;
      for (FAR struct littlefs_page_s *p = node->pages; p; p = p->next)
        {
          DEBUGASSERT(count < LITTLEFS_MAX_PAGES);
          ranges[count].off = p->off;
          ranges[count].size = p->valid;
          ranges[count].buffer = p->data;
          count++;
        }

      ret = littlefs_convert_result(lfs_file_commit_updates(&fs->lfs,
                                      &node->file, ranges, count));
      if (ret < 0)
        {
          return littlefs_node_error(node, ret);
        }

      littlefs_drop_pages(node);
    }
#endif

  ret = littlefs_convert_result(lfs_file_sync(&fs->lfs, &node->file));
  return littlefs_node_error(node, ret);
}

static int littlefs_refresh_attr(FAR struct littlefs_mountpt_s *fs,
                                 FAR struct littlefs_node_s *node,
                                 FAR const char *path)
{
  struct littlefs_attr_s attr;
  int ret = littlefs_convert_result(lfs_getattr(&fs->lfs, path, 0,
                                               &attr, sizeof(attr)));
  if (ret == -ENODATA)
    {
      memset(&attr, 0, sizeof(attr));
      attr.at_mode = S_IRWXG | S_IRWXU | S_IRWXO;
      ret = 0;
    }

  if (ret >= 0)
    {
      node->attr = attr;
      ret = 0;
    }

  return ret;
}

static bool littlefs_can_cache(FAR struct littlefs_node_s *node,
                               off_t pos, size_t size)
{
#ifdef CONFIG_FS_LITTLEFS_COALESCE
  return !(node->file.flags & LFS_F_INLINE) && !littlefs_unlinked(node) &&
         pos >= 0 && (uint64_t)pos <= node->file.ctz.size &&
         size <= node->file.ctz.size - (lfs_off_t)pos;
#else
  UNUSED(node);
  UNUSED(pos);
  UNUSED(size);
  return false;
#endif
}

#ifdef CONFIG_FS_LITTLEFS_COALESCE
static FAR uint8_t *littlefs_new_page(FAR struct littlefs_mountpt_s *fs)
{
  FAR uint8_t *data = coalesce_page_alloc();
  if (data == NULL)
    {
      /* The cap is global across mounts. Only evict this mount while its
       * mutex is held; a different mount owning the pool may cause ENOMEM.
       * Never acquire another mount's lock or discard its dirty data.
       */

      for (FAR struct littlefs_node_s *n = fs->nodes; n; n = n->next)
        {
          if (n->pages && !n->error && littlefs_node_sync(fs, n) == 0)
            {
              data = coalesce_page_alloc();
              if (data)
                {
                  break;
                }
            }
        }
    }

  return data;
}
#endif

static ssize_t littlefs_cache_write(FAR struct littlefs_mountpt_s *fs,
                                    FAR struct littlefs_node_s *node,
                                    lfs_off_t pos, FAR const char *buffer,
                                    size_t size)
{
#ifdef CONFIG_FS_LITTLEFS_COALESCE
  size_t done = 0;
  int ret = 0;
  while (done < size)
    {
      lfs_off_t off = (pos + done) & ~(LITTLEFS_PAGE_SIZE - 1);
      lfs_size_t within = pos + done - off;
      size_t n = lfs_min(size - done, LITTLEFS_PAGE_SIZE - within);
      FAR struct littlefs_page_s **link = &node->pages;
      FAR struct littlefs_page_s *page;
      while (*link && (*link)->off < off)
        {
          link = &(*link)->next;
        }

      page = *link;
      if (page == NULL || page->off != off)
        {
          FAR uint8_t *data = littlefs_new_page(fs);
          if (node->error)
            {
              if (data)
                {
                  coalesce_page_free(data);
                }

              ret = node->error;
              break;
            }

          if (data == NULL)
            {
              ret = -ENOMEM;
              break;
            }

          /* Eviction may have freed the old list and invalidated link. */

          link = &node->pages;
          while (*link && (*link)->off < off)
            {
              link = &(*link)->next;
            }

          page = fs_heap_zalloc(sizeof(*page));
          if (page == NULL)
            {
              coalesce_page_free(data);
              ret = -ENOMEM;
              break;
            }

          page->data = data;
          page->off = off;
          page->valid = lfs_min(LITTLEFS_PAGE_SIZE, node->file.ctz.size - off);
          if (within != 0 || n != page->valid)
            {
              ret = littlefs_convert_result(lfs_file_seek(&fs->lfs,
                                            &node->file, off, LFS_SEEK_SET));
              if (ret >= 0)
                {
                  ret = littlefs_convert_result(lfs_file_read(&fs->lfs,
                              &node->file, data, page->valid));
                }

              if (ret != (int)page->valid)
                {
                  ret = ret < 0 ? ret : -EIO;
                  coalesce_page_free(data);
                  fs_heap_free(page);
                  littlefs_node_error(node, ret);
                  break;
                }
            }

          page->next = *link;
          *link = page;
        }

      memcpy(page->data + within, buffer + done, n);
      done += n;
    }

  return done ? (ssize_t)done : ret;
#else
  UNUSED(fs);
  UNUSED(node);
  UNUSED(pos);
  UNUSED(buffer);
  UNUSED(size);
  return -ENOSYS;
#endif
}

static ssize_t littlefs_node_read(FAR struct littlefs_mountpt_s *fs,
                                  FAR struct littlefs_node_s *node,
                                  lfs_off_t pos, FAR char *buffer, size_t size)
{
  size_t done = 0;
  lfs_soff_t length = lfs_file_size(&fs->lfs, &node->file);
  if (length < 0)
    {
      return littlefs_convert_result(length);
    }

  if (pos >= (lfs_off_t)length)
    {
      return 0;
    }

  size = lfs_min(size, (lfs_off_t)length - pos);
  while (done < size)
    {
      lfs_off_t off = (pos + done) & ~(LITTLEFS_PAGE_SIZE - 1);
      size_t within = pos + done - off;
      size_t n = lfs_min(size - done, LITTLEFS_PAGE_SIZE - within);
      FAR struct littlefs_page_s *p;
      for (p = node->pages; p && p->off < off; p = p->next)
        {
        }

      if (p && p->off == off)
        {
          memcpy(buffer + done, p->data + within, n);
        }
      else
        {
          int ret = littlefs_convert_result(lfs_file_seek(&fs->lfs,
                               &node->file, pos + done, LFS_SEEK_SET));
          if (ret >= 0)
            {
              ret = littlefs_convert_result(lfs_file_read(&fs->lfs,
                                              &node->file, buffer + done, n));
            }

          if (ret != (int)n)
            {
              ret = ret < 0 ? ret : -EIO;
              littlefs_node_error(node, ret);
              return done ? (ssize_t)done : ret;
            }
        }

      done += n;
    }

  return done;
}
