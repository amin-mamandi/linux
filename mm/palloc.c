/**
* kernel/palloc.c
*
* Color Aware Physical Memory Allocator User-Space Information
*
*/

#include <linux/types.h>
#include <linux/cgroup.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/palloc.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/bitmap.h>
#include <linux/module.h>

/**
 * Check if a page is compliant with the policy defined for the given vma
 */
#ifdef CONFIG_CGROUP_PALLOC

#define MAX_LINE_LEN (6 * 128)

/**
 * Type of files in a palloc group
 * FILE_PALLOC - contains list of palloc bins allowed
 */
typedef enum {
	FILE_PALLOC,
#ifdef CONFIG_DETMEM_PALLOC
	FILE_PALLOC_DM
#endif
} palloc_filetype_t;

/**
 * Retrieve the palloc group corresponding to this cgroup container
 */
struct palloc *cgroup_ph(struct cgroup *cgrp)
{
	return container_of(cgrp->subsys[palloc_cgrp_id], struct palloc, css);
}

struct palloc *ph_from_subsys(struct cgroup_subsys_state *subsys)
{
	return container_of(subsys, struct palloc, css);
}

/**
 * Common write function for files in palloc cgroup
 */
static int update_bitmask(unsigned long *bitmap, const char *buf, int maxbits)
{
	int retval = 0;

	if (!*buf)
		bitmap_clear(bitmap, 0, maxbits);
	else
		retval = bitmap_parselist(buf, bitmap, maxbits);

	return retval;
}

static ssize_t palloc_file_write(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off)
{
	struct cgroup_subsys_state *css;
	struct cftype *cft;
	int retval = 0;
	struct palloc *ph;

	css = of_css(of);
	cft = of_cft(of);
	ph = container_of(css, struct palloc, css);

	switch (cft->private) {
		case FILE_PALLOC:
			retval = update_bitmask(ph->cmap, buf, palloc_bins());
			printk(KERN_INFO "Bins : %s\n", buf);
			break;
#ifdef CONFIG_DETMEM_PALLOC
		case FILE_PALLOC_DM:
			retval = update_bitmask(ph->dm_cmap, buf, palloc_bins());
			printk(KERN_INFO "DM Bins : %s\n", buf);
			break;
#endif
		default:
			retval = -EINVAL;
			break;
	}

	return retval? :nbytes;
}

static int palloc_file_read(struct seq_file *sf, void *v)
{
	struct cgroup_subsys_state *css = seq_css(sf);
	struct cftype *cft = seq_cft(sf);
	struct palloc *ph = container_of(css, struct palloc, css);
	char *page;
	ssize_t retval = 0;
	char *s;

	if (!(page = (char *)__get_free_page( __GFP_ZERO)))
		return -ENOMEM;

	s = page;

	switch (cft->private) {
		case FILE_PALLOC:
			s += scnprintf(s, PAGE_SIZE, "%*pbl", (int)palloc_bins(), ph->cmap);
			*s++ = '\n';
			printk(KERN_INFO "Bins : %s", page);
			break;
#ifdef CONFIG_DETMEM_PALLOC
		case FILE_PALLOC_DM:
			s += scnprintf(s, PAGE_SIZE - (s - page), "%*pbl", (int)palloc_bins(), ph->dm_cmap);
			*s++ = '\n';  // Add newline character
			printk(KERN_INFO "DM Bins : %s", page);  // Print the whole buffer
			break;
#endif
		default:
			retval = -EINVAL;
			goto out;
	}

	seq_printf(sf, "%s", page);

out:
	free_page((unsigned long)page);
	return retval;
}

/**
 * struct cftype : handler definitions for cgroup control files
 *
 * for the common functions, 'private' gives the type of the file
 */
static struct cftype files[] = {
	{
		.name 		= "bins",
		.seq_show	= palloc_file_read,
		.write		= palloc_file_write,
		.max_write_len	= MAX_LINE_LEN,
		.private	= FILE_PALLOC,
	},
#ifdef CONFIG_DETMEM_PALLOC
	{
		.name       = "dm_bins",
		.seq_show   = palloc_file_read,
		.write      = palloc_file_write,
		.max_write_len  = MAX_LINE_LEN,
		.private    = FILE_PALLOC_DM,
	},
#endif
	{}
};


/**
 * palloc_create - create a palloc group
 */
static struct cgroup_subsys_state *palloc_create(struct cgroup_subsys_state *css)
{
	struct palloc *ph_child;

	/* kzalloc: every bitmap in the group must start out clear. */
	ph_child = kzalloc(sizeof(struct palloc), GFP_KERNEL);

	if (!ph_child)
		return ERR_PTR(-ENOMEM);

	return &ph_child->css;
}

/**
 * Destroy an existing palloc group
 */
static void palloc_destroy(struct cgroup_subsys_state *css)
{
	struct palloc *ph = container_of(css, struct palloc, css);

	kfree(ph);
}

struct cgroup_subsys palloc_cgrp_subsys = {
	.css_alloc	= palloc_create,
	.css_free	= palloc_destroy,
	.dfl_cftypes	= files,
	.legacy_cftypes	= files,
};

#endif /* CONFIG_CGROUP_PALLOC */
