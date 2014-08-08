#include <linux/cgroup.h>
#include <linux/res_counter.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/shmem_fs.h>
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

static struct vm_cgroup *vm_cgroup_from_task(struct task_struct *p)
{
	return vm_cgroup_from_css(task_css(p, vm_subsys_id));
}

static struct vm_cgroup *get_vm_cgroup_from_task(struct task_struct *p)
{
	struct vm_cgroup *vmcg;

	rcu_read_lock();
	do {
		vmcg = vm_cgroup_from_task(p);
	} while (!css_tryget_online(&vmcg->css));
	rcu_read_unlock();

	return vmcg;
}

void mm_init_vm_cgroup(struct mm_struct *mm, struct task_struct *p)
{
	if (!vm_cgroup_disabled())
		mm->vmcg = get_vm_cgroup_from_task(p);
}

void mm_release_vm_cgroup(struct mm_struct *mm)
{
	struct vm_cgroup *vmcg = mm->vmcg;

	if (vmcg)
		css_put(&vmcg->css);
}

static int vm_cgroup_do_charge(struct vm_cgroup *vmcg,
			       unsigned long nr_pages)
{
	unsigned long val = nr_pages << PAGE_SHIFT;
	struct res_counter *fail_res;

	return res_counter_charge(&vmcg->res, val, &fail_res);
}

static void vm_cgroup_do_uncharge(struct vm_cgroup *vmcg,
				  unsigned long nr_pages)
{
	unsigned long val = nr_pages << PAGE_SHIFT;

	res_counter_uncharge(&vmcg->res, val);
}

int vm_cgroup_charge_memory_mm(struct mm_struct *mm, unsigned long nr_pages)
{
	struct vm_cgroup *vmcg = mm->vmcg;

	if (vmcg)
		return vm_cgroup_do_charge(vmcg, nr_pages);
	return 0;
}

void vm_cgroup_uncharge_memory_mm(struct mm_struct *mm, unsigned long nr_pages)
{
	struct vm_cgroup *vmcg = mm->vmcg;

	if (vmcg)
		vm_cgroup_do_uncharge(vmcg, nr_pages);
}

#ifdef CONFIG_SHMEM
void shmem_init_vm_cgroup(struct shmem_inode_info *info)
{
	if (!vm_cgroup_disabled())
		info->vmcg = get_vm_cgroup_from_task(current);
}

void shmem_release_vm_cgroup(struct shmem_inode_info *info)
{
	struct vm_cgroup *vmcg = info->vmcg;

	if (vmcg)
		css_put(&vmcg->css);
}

int vm_cgroup_charge_shmem(struct shmem_inode_info *info,
			   unsigned long nr_pages)
{
	struct vm_cgroup *vmcg = info->vmcg;

	if (vmcg)
		return vm_cgroup_do_charge(vmcg, nr_pages);
	return 0;
}

void vm_cgroup_uncharge_shmem(struct shmem_inode_info *info,
			      unsigned long nr_pages)
{
	struct vm_cgroup *vmcg = info->vmcg;

	if (vmcg)
		vm_cgroup_do_uncharge(info->vmcg, nr_pages);
}
#endif /* CONFIG_SHMEM */

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

static int vm_cgroup_write(struct cgroup_subsys_state *css,
		struct cftype *cft, u64 value)
{
	struct vm_cgroup *vmcg = vm_cgroup_from_css(css);

	if (vm_cgroup_is_root(vmcg))
		return -EINVAL;

	return res_counter_set_limit(&vmcg->res, value);
}

static int vm_cgroup_reset(struct cgroup_subsys_state *css,
		struct cftype *cft, u64 value)
{
	struct vm_cgroup *vmcg= vm_cgroup_from_css(css);
	int memb = cft->private;

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
	return 0;
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
		.write_u64 = vm_cgroup_reset,
		.read_u64 = vm_cgroup_read_u64,
	},
	{
		.name = "limit_in_bytes",
		.private = RES_LIMIT,
		.write_u64 = vm_cgroup_write,
		.read_u64 = vm_cgroup_read_u64,
	},
	{
		.name = "failcnt",
		.private = RES_FAILCNT,
		.write_u64 = vm_cgroup_reset,
		.read_u64 = vm_cgroup_read_u64,
	},
	{ },	/* terminate */
};

struct cgroup_subsys vm_subsys = {
	.name = "vm",
	.css_alloc = vm_cgroup_css_alloc,
	.css_free = vm_cgroup_css_free,
	.base_cftypes = vm_cgroup_files,
	.subsys_id = vm_subsys_id,
};
