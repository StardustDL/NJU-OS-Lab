#include <common.h>
#include <fs.h>

void inode_free(inode *node)
{
  if (node == NULL)
    return;
  if (node->ops != NULL)
    pmm->free(node->ops);
  if (node->info != NULL)
  {
    if (node->info->path != NULL)
      pmm->free((char *)node->info->path);
    pmm->free(node->info);
  }
  pmm->free(node);
}

void fslist_node_free(fslist *node)
{
  if (node == NULL)
    return;
  if (node->name != NULL)
    pmm->free((void *)node->name);
  pmm->free(node);
}