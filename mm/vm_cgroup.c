#include <linux/cgroup.h>
#include <linux/res_counter.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vm_cgroup.h>

struct vm_cgroup {
	struct cgroup_subsys_state css;

	/*
	 * The counter to account for vm usage.
	 */
	struct res_counter res;
};

static struct vm_cgroup *root_vm_cgroup __read_mostly;

static inline bool vm_cgroup_is_root(struct vm_cgroup *vmcg)
{
	return vmcg == root_vm_cgroup;
}

static struct vm_cgroup *vm_cgroup_from_css(struct cgroup_subsys_state *s)
{
	return s ? container_of(s, struct vm_cgroup, css) : NULL;
}

static struct cgroup_subsys_state *
vm_cgroup_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct vm_cgroup *parent = vm_cgroup_from_css(parent_css);
	struct vm_cgroup *vmcg;

	vmcg = kzalloc(sizeof(*vmcg), GFP_KERNEL);
	if (!vmcg)
		return ERR_PTR(-ENOMEM);

	res_counter_init(&vmcg->res, parent ? &parent->res : NULL);

	if (!parent)
		root_vm_cgroup = vmcg;

	return &vmcg->css;
}

static void vm_cgroup_css_free(struct cgroup_subsys_state *css)
{
	struct vm_cgroup *vmcg = vm_cgroup_from_css(css);

	kfree(vmcg);
}

static u64 vm_cgroup_read_u64(struct cgroup_subsys_state *css,
			      struct cftype *cft)
{
	struct vm_cgroup *vmcg = vm_cgroup_from_css(css);
	int memb = cft->private;

	return res_counter_read_u64(&vmcg->res, memb);
}

static ssize_t vm_cgroup_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	struct vm_cgroup *vmcg = vm_cgroup_from_css(of_css(of));
	unsigned long long val;
	int ret;

	if (vm_cgroup_is_root(vmcg))
		return -EINVAL;

	buf = strstrip(buf);
	ret = res_counter_memparse_write_strategy(buf, &val);
	if (ret)
		return ret;

	ret = res_counter_set_limit(&vmcg->res, val);
	return ret ?: nbytes;
}

static ssize_t vm_cgroup_reset(struct kernfs_open_file *of, char *buf,
			       size_t nbytes, loff_t off)
{
	struct vm_cgroup *vmcg= vm_cgroup_from_css(of_css(of));
	int memb = of_cft(of)->private;

	switch (memb) {
	case RES_MAX_USAGE:
		res_counter_reset_max(&vmcg->res);
		break;
	case RES_FAILCNT:
		res_counter_reset_failcnt(&vmcg->res);
		break;
	default:
		BUG();
	}
	return nbytes;
}

static struct cftype vm_cgroup_files[] = {
	{
		.name = "usage_in_bytes",
		.private = RES_USAGE,
		.read_u64 = vm_cgroup_read_u64,
	},
	{
		.name = "max_usage_in_bytes",
		.private = RES_MAX_USAGE,
		.write = vm_cgroup_reset,
		.read_u64 = vm_cgroup_read_u64,
	},
	{
		.name = "limit_in_bytes",
		.private = RES_LIMIT,
		.write = vm_cgroup_write,
		.read_u64 = vm_cgroup_read_u64,
	},
	{
		.name = "failcnt",
		.private = RES_FAILCNT,
		.write = vm_cgroup_reset,
		.read_u64 = vm_cgroup_read_u64,
	},
	{ },	/* terminate */
};

struct cgroup_subsys vm_cgrp_subsys = {
	.css_alloc = vm_cgroup_css_alloc,
	.css_free = vm_cgroup_css_free,
	.base_cftypes = vm_cgroup_files,
};
