/* filescontrol.c - Cgroup controller for open file handles.
 *
 * Copyright 2014 Google Inc.
 * Author: Brian Makin <merimus@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/res_counter.h>
#include <linux/filescontrol.h>
#include <linux/cgroup.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/fdtable.h>

struct cgroup_subsys files_subsys __read_mostly;
EXPORT_SYMBOL(files_subsys);

struct files_cgroup {
	struct cgroup_subsys_state css;
	struct res_counter open_handles;
};

static inline struct files_cgroup *css_fcg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct files_cgroup, css) : NULL;
}

static inline struct res_counter *
css_res_open_handles(struct cgroup_subsys_state *css)
{
	return &css_fcg(css)->open_handles;
}

static inline struct files_cgroup *
files_cgroup_from_files(struct files_struct *files)
{
	return files->files_cgroup;
}

static struct cgroup_subsys_state *
files_cgroup_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct files_cgroup *fcg;

	fcg = kzalloc(sizeof(*fcg), GFP_KERNEL);
	if (!fcg)
		goto out;

	if (!parent_css) {
		res_counter_init(&fcg->open_handles, NULL);
		res_counter_set_limit(&fcg->open_handles, get_max_files());
	} else {
		struct files_cgroup *parent_fcg = css_fcg(parent_css);
		res_counter_init(&fcg->open_handles, &parent_fcg->open_handles);
		res_counter_set_limit(&fcg->open_handles,
				res_counter_read_u64(&parent_fcg->open_handles,
						RES_LIMIT));
	}
	return &fcg->css;

out:
	return ERR_PTR(-ENOMEM);
}

static void files_cgroup_css_free(struct cgroup_subsys_state *css)
{
	kfree(css_fcg(css));
}

u64 files_cgroup_count_fds(struct files_struct *files)
{
	int i;
	struct fdtable *fdt;
	int retval = 0;

	fdt = files_fdtable(files);
	for (i = 0; i < DIV_ROUND_UP(fdt->max_fds, BITS_PER_LONG); i++)
		retval += hweight64((__u64)fdt->open_fds[i]);
	return retval;
}

static u64 files_in_taskset(struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css = NULL;
	struct task_struct *task;
	u64 files = 0;
	cgroup_taskset_for_each(task, css, tset) {
		if (!thread_group_leader(task))
			continue;

		task_lock(task);
		files += files_cgroup_count_fds(task->files);
		task_unlock(task);
	}
	return files;
}

/*
 * If attaching this cgroup would overcommit the resource then deny
 * the attach.
 */
static int files_cgroup_can_attach(struct cgroup_subsys_state *css,
				struct cgroup_taskset *tset)
{
	u64 files = files_in_taskset(tset);
	if (res_counter_margin(css_res_open_handles(css)) < files)
		return -ENOMEM;
	return 0;
}

/*
 * If resource counts have gone up between can_attach and attach then
 * this may overcommit resources.  In that case just deny further allocation
 * until the resource usage drops.
 */
static void files_cgroup_attach(struct cgroup_subsys_state *to_css,
				struct cgroup_taskset *tset)
{
	u64 num_files;
	struct task_struct *task = cgroup_taskset_first(tset);
	struct cgroup_subsys_state *from_css;
	struct res_counter *from_res;
	struct res_counter *to_res = css_res_open_handles(to_css);
	struct res_counter *fail_res;
	struct files_struct *files;

	task_lock(task);
	files = task->files;
	if (!files) {
		task_unlock(task);
		return;
	}

	from_css = &files_cgroup_from_files(files)->css;
	from_res = css_res_open_handles(from_css);

	spin_lock(&files->file_lock);
	num_files = files_cgroup_count_fds(files);
	res_counter_uncharge(from_res, num_files);
	css_put(from_css);

	if (res_counter_charge(to_res, num_files, &fail_res))
		pr_err("Open files limit overcommited\n");
	css_get(to_css);

	task->files->files_cgroup = css_fcg(to_css);
	spin_unlock(&files->file_lock);
	task_unlock(task);
}

int files_cgroup_alloc_fd(struct files_struct *files, u64 n)
{
	struct res_counter *fail_res;
	struct files_cgroup *files_cgroup = files_cgroup_from_files(files);

	if (res_counter_charge(&files_cgroup->open_handles, n, &fail_res))
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(files_cgroup_alloc_fd);

void files_cgroup_unalloc_fd(struct files_struct *files, u64 n)
{
	struct files_cgroup *files_cgroup = files_cgroup_from_files(files);

	res_counter_uncharge(&files_cgroup->open_handles, n);
}
EXPORT_SYMBOL(files_cgroup_unalloc_fd);

static u64 files_limit_read(struct cgroup_subsys_state *css,
			struct cftype *cft)
{
	struct files_cgroup *fcg = css_fcg(css);
	return res_counter_read_u64(&fcg->open_handles, RES_LIMIT);
}

static int files_limit_write(struct cgroup_subsys_state *css,
			struct cftype *cft, u64 value)
{
	struct files_cgroup *fcg = css_fcg(css);
	return res_counter_set_limit(&fcg->open_handles, value);
}

static u64 files_usage_read(struct cgroup_subsys_state *css,
			struct cftype *cft)
{
	struct files_cgroup *fcg = css_fcg(css);
	return res_counter_read_u64(&fcg->open_handles, RES_USAGE);
}

static struct cftype files[] = {
	{
		.name = "limit",
		.read_u64 = files_limit_read,
		.write_u64 = files_limit_write,
	},
	{
		.name = "usage",
		.read_u64 = files_usage_read,
	},
	{ }
};

struct cgroup_subsys files_subsys = {
	.name = "files",
	.css_alloc = files_cgroup_css_alloc,
	.css_free = files_cgroup_css_free,
	.can_attach = files_cgroup_can_attach,
	.attach = files_cgroup_attach,
	.base_cftypes = files,
	.subsys_id = files_subsys_id,
};

void files_cgroup_assign(struct files_struct *files)
{
	struct task_struct *tsk = current;
	struct cgroup_subsys_state *css;

	task_lock(tsk);
	css = task_css(tsk, files_subsys_id);
	css_get(css);
	files->files_cgroup = container_of(css, struct files_cgroup, css);
	task_unlock(tsk);
}

void files_cgroup_remove(struct files_struct *files)
{
	struct task_struct *tsk = current;
	struct files_cgroup *fcg;

	task_lock(tsk);
	spin_lock(&files->file_lock);
	fcg = files_cgroup_from_files(files);
	css_put(&fcg->css);
	spin_unlock(&files->file_lock);
	task_unlock(tsk);
}
