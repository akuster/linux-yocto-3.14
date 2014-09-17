#ifndef _LINUX_VM_CGROUP_H
#define _LINUX_VM_CGROUP_H

struct mm_struct;
struct shmem_inode_info;

#ifdef CONFIG_CGROUP_VM
static inline bool vm_cgroup_disabled(void)
{
	if (vm_subsys.disabled)
		return true;
	return false;
}

extern void mm_init_vm_cgroup(struct mm_struct *mm, struct task_struct *p);
extern void mm_release_vm_cgroup(struct mm_struct *mm);
extern int vm_cgroup_charge_memory_mm(struct mm_struct *mm,
				      unsigned long nr_pages);
extern void vm_cgroup_uncharge_memory_mm(struct mm_struct *mm,
					 unsigned long nr_pages);

#ifdef CONFIG_SHMEM
extern void shmem_init_vm_cgroup(struct shmem_inode_info *info);
extern void shmem_release_vm_cgroup(struct shmem_inode_info *info);
extern int vm_cgroup_charge_shmem(struct shmem_inode_info *info,
				  unsigned long nr_pages);
extern void vm_cgroup_uncharge_shmem(struct shmem_inode_info *info,
				     unsigned long nr_pages);
#endif

#else /* !CONFIG_CGROUP_VM */
static inline bool vm_cgroup_disabled(void)
{
	return true;
}

static inline void mm_init_vm_cgroup(struct mm_struct *mm,
				     struct task_struct *p)
{
}

static inline void mm_release_vm_cgroup(struct mm_struct *mm)
{
}

static inline int vm_cgroup_charge_memory_mm(struct mm_struct *mm,
					     unsigned long nr_pages)
{
	return 0;
}

static inline void vm_cgroup_uncharge_memory_mm(struct mm_struct *mm,
						unsigned long nr_pages)
{
}

#ifdef CONFIG_SHMEM
static inline void shmem_init_vm_cgroup(struct shmem_inode_info *info)
{
}

static inline void shmem_release_vm_cgroup(struct shmem_inode_info *info)
{
}

static inline int vm_cgroup_charge_shmem(struct shmem_inode_info *info,
					 unsigned long nr_pages)
{
	return 0;
}

static inline void vm_cgroup_uncharge_shmem(struct shmem_inode_info *info,
					    unsigned long nr_pages)
{
}
#endif

#endif /* CONFIG_CGROUP_VM */

#endif /* _LINUX_VM_CGROUP_H */
