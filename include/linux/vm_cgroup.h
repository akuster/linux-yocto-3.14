#ifndef _LINUX_VM_CGROUP_H
#define _LINUX_VM_CGROUP_H

#ifdef CONFIG_CGROUP_VM
static inline bool vm_cgroup_disabled(void)
{
	if (vm_subsys.disabled)
		return true;
	return false;
}
#else /* !CONFIG_CGROUP_VM */
static inline bool vm_cgroup_disabled(void)
{
	return true;
}
#endif /* CONFIG_CGROUP_VM */

#endif /* _LINUX_VM_CGROUP_H */
