#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define MIN(A,B) (((A) < (B))?(A):(B))

#define DEFAULT_MODE 0775
#define NB_INODE 100
#define INODE_MAX_SIZE 1000
#define BUFFER_SIZE ((NB_INODE)*(INODE_MAX_SIZE))

#define DUMMY_OPS(NAME,...) static int tfs_##NAME(__VA_ARGS__) { \
  pr_debug(#NAME); \
  pr_debug("\n");  \
  return 0;        \
  }

static char buffer[BUFFER_SIZE];
static struct inode * inode_tab[NB_INODE];

DUMMY_OPS(rmdir,struct inode * i,struct dentry * d)
DUMMY_OPS(rename,struct inode * i, struct dentry *d, struct inode *i2, struct dentry *d2, unsigned int ui)
DUMMY_OPS(symlink,struct inode * i,struct dentry * d,const char * c)

void put_next_inode(struct inode * dir)
{
  static size_t i = 0;

  pr_debug("Next inode : %lu\n", i);

  if(i >= NB_INODE) pr_err("Can't create more inode\n");
  else {
    inode_tab[i++] = dir;
  }
}

static struct inode * tfs_get_inode(struct super_block *sb,
				     const struct inode * dir,
				     umode_t mode,
				     dev_t dev);

static int tfs_mknod(struct inode * dir, struct dentry * dentry, umode_t mode, dev_t dev)
{
  struct inode * inode = tfs_get_inode(dir->i_sb,dir,mode,dev);
  struct timespec64 ts;
  int error = -ENOSPC;

  pr_debug("TFS : mknod\n");
  
  if(inode) {
    d_instantiate(dentry,inode);
    dget(dentry);
    error = 0;
    ktime_get_ts64(&ts);
    dir->i_mtime = dir->i_ctime = ts;
    put_next_inode(inode);
  }

  return error;
}

static int tfs_create(struct inode * dir, struct dentry * dentry, umode_t mode, bool excl)
{
  pr_debug("TFS : create\n");
  return tfs_mknod(dir,dentry, mode | S_IFREG, 0);
}

static int tfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
  int retval = tfs_mknod(dir,dentry,mode | S_IFDIR, 0);
  pr_debug("TFS : mkdir\n");
  if(!retval) inc_nlink(dir);

  return retval;
}

static struct inode_operations tfs_dir_inode_operations = {
							    .create = tfs_create,
							    .lookup = simple_lookup,
							    .mkdir  = tfs_mkdir,
							    .mknod  = tfs_mknod,
							    .rmdir  = tfs_rmdir,
							    .rename = tfs_rename,
							    .symlink= tfs_symlink,
};

int tfs_file_open(struct inode * inode, struct file * file)
{
  int i = 0;

  pr_debug("TFS : i_ino %ld\n", inode->i_ino);

  while(i < NB_INODE && inode != inode_tab[i]) ++i;
  
  file->private_data = &(buffer[i*INODE_MAX_SIZE]);
  return 0;
}

ssize_t tfs_file_read(struct file * file, char __user *user_buf,
		       size_t count, loff_t * ppos)
{
  unsigned long err;
  loff_t pos = *ppos;

  count = MIN(count,INODE_MAX_SIZE);
  *ppos += count;

  pr_debug("TFS : read\n");

  if(pos < 0)
    return -EINVAL;
  if(pos > 0) {
    return 0;
  }

  if((err=copy_to_user(user_buf, file->private_data,count))) {
    pr_debug("Err read %lu bytes could not be read on %lu\n",err,count);
    return -EFAULT;
  }
  else
    return count;
}

ssize_t tfs_file_write(struct file * file,
			const char __user *user_buf,
			size_t count, loff_t *ppos)
{
  loff_t pos = *ppos;

  pr_debug("Count %lu pos %lld\n", count, pos);
  
  count = MIN(count,INODE_MAX_SIZE);

  *ppos = pos + count;

  pr_debug("TFS : write (%s)\n", user_buf);
  
  if(pos < 0)
    return -EINVAL;
  
  if(copy_from_user(file->private_data + pos, user_buf,count))
    return -EFAULT;
  else
    return count;
}

static struct file_operations tfs_file_operations = {
						      .open = tfs_file_open,
						      .read = tfs_file_read,
						      .write = tfs_file_write,
};

static struct inode * tfs_get_inode(struct super_block *sb,
				     const struct inode * dir,
				     umode_t mode,
				     dev_t dev)
{
  struct inode * inode = new_inode(sb);

  pr_debug("TFS : get_inode\n");
  
  if(inode) {
    inode->i_ino = get_next_ino();
    inode_init_owner(inode,dir,mode);

    inode->i_op = &tfs_dir_inode_operations;
    if(mode & S_IFDIR)
      inode->i_fop = &simple_dir_operations;
    else
      inode->i_fop = &tfs_file_operations;
  }

  return inode;
}

static const struct super_operations tfs_ops = {
						 
};

static int tfs_file_super(struct super_block * sb, void * data, int silent)
{
  struct inode * inode;

  sb->s_op = &tfs_ops;
  inode = tfs_get_inode(sb,NULL,S_IFDIR | DEFAULT_MODE, 0);
  sb->s_root = d_make_root(inode);
  if(!sb->s_root) 
    pr_debug("cannot get dentry root\n");

  pr_debug("Mounting ok\n");
  
  return 0;
}

struct dentry *tfs_mount(struct file_system_type *fs_type,
			  int flags,
			  const char *dev_name,
			  void *data)
{
  pr_debug("Mounting ..");
  return mount_nodev(fs_type,flags,data,tfs_file_super); // Mount not on a physical device
}

static void tfs_kill_sb(struct super_block *sb)
{
  kfree(sb->s_fs_info);
  kill_litter_super(sb);
  pr_debug("umount tfs\n");
}

static struct file_system_type tfs_type = {
					    .name     = "tfs",
					    .mount    = tfs_mount,
					    .kill_sb  = tfs_kill_sb,
					    .fs_flags = FS_USERNS_MOUNT,
};

static int __init init_tfs(void)
{
  const int ret = register_filesystem(&tfs_type);

  if(ret) {
    pr_err("Can't register tfs\n");
    return ret;
  }

  pr_debug("tfs registered\n");

  return 0;
}

static void __exit exit_tfs(void)
{
  const int ret = unregister_filesystem(&tfs_type);

  if(ret) pr_err("Can't unergister tfs");
  else    pr_debug("tfs unregistered\n");
}

module_init(init_tfs);
module_exit(exit_tfs);
MODULE_AUTHOR("Sharkalash");
MODULE_LICENSE("GPL");

