/*
 * Copyright 2016-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * dirs.cpp -- unit test for directories
 */

#include "pmemfile_test.hpp"

static size_t ops = 100;

class dirs : public pmemfile_test {
public:
	dirs() : pmemfile_test(256 * 1024 * 1024)
	{
		// XXX
		test_empty_dir_on_teardown = false;
	}
};

static const char *
timespec_to_str(const pmemfile_timespec_t *t)
{
	time_t sec = t->tv_sec;
	char *s = asctime(localtime(&sec));
	s[strlen(s) - 1] = 0;
	return s;
}

static void
dump_stat(pmemfile_stat_t *st, const char *path)
{
	T_OUT("path:       %s\n", path);
	T_OUT("st_dev:     0x%lx\n", st->st_dev);
	T_OUT("st_ino:     %ld\n", st->st_ino);
	T_OUT("st_mode:    0%o\n", st->st_mode);
	T_OUT("st_nlink:   %lu\n", st->st_nlink);
	T_OUT("st_uid:     %u\n", st->st_uid);
	T_OUT("st_gid:     %u\n", st->st_gid);
	T_OUT("st_rdev:    0x%lx\n", st->st_rdev);
	T_OUT("st_size:    %ld\n", st->st_size);
	T_OUT("st_blksize: %ld\n", st->st_blksize);
	T_OUT("st_blocks:  %ld\n", st->st_blocks);
	T_OUT("st_atim:    %ld.%.9ld, %s\n", st->st_atim.tv_sec,
	      st->st_atim.tv_nsec, timespec_to_str(&st->st_atim));
	T_OUT("st_mtim:    %ld.%.9ld, %s\n", st->st_mtim.tv_sec,
	      st->st_mtim.tv_nsec, timespec_to_str(&st->st_mtim));
	T_OUT("st_ctim:    %ld.%.9ld, %s\n", st->st_ctim.tv_sec,
	      st->st_ctim.tv_nsec, timespec_to_str(&st->st_ctim));
	T_OUT("---");
}

struct linux_dirent64 {
	uint64_t d_ino;
	uint64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

#define VAL_EXPECT_EQ(v1, v2)                                                  \
	do {                                                                   \
		if ((v1) != (v2)) {                                            \
			ADD_FAILURE() << (v1) << " != " << (v2);               \
			pmemfile_close(pfp, f);                                \
			return false;                                          \
		}                                                              \
	} while (0)

static bool
list_files(PMEMfilepool *pfp, const char *dir, size_t expected_files,
	   int just_count, const char *name)
{
	T_OUT("\"%s\" start\n", name);
	PMEMfile *f = pmemfile_open(pfp, dir,
				    PMEMFILE_O_DIRECTORY | PMEMFILE_O_RDONLY);
	if (!f) {
		EXPECT_NE(f, nullptr);
		return false;
	}

	char buf[32 * 1024];
	char path[PMEMFILE_PATH_MAX];
	struct linux_dirent64 *d = (struct linux_dirent64 *)buf;
	int r = pmemfile_getdents64(pfp, f, d, sizeof(buf));
	size_t num_files = 0;
	if (r < 0) {
		EXPECT_GE(r, 0);
		pmemfile_close(pfp, f);
		return false;
	}

	while ((uintptr_t)d < (uintptr_t)&buf[r]) {
		num_files++;
		if (!just_count) {
			T_OUT("ino: 0x%lx, off: 0x%lx, len: %d, type: %d, "
			      "name: \"%s\"\n",
			      d->d_ino, d->d_off, d->d_reclen, d->d_type,
			      d->d_name);
			sprintf(path, "/%s/%s", dir, d->d_name);

			pmemfile_stat_t st;
			int ret = pmemfile_stat(pfp, path, &st);
			VAL_EXPECT_EQ(ret, 0);
			dump_stat(&st, path);
		}
		d = (struct linux_dirent64 *)(((char *)d) + d->d_reclen);
	}

	T_OUT("\"%s\" end\n", name);
	VAL_EXPECT_EQ(num_files, expected_files);

	pmemfile_close(pfp, f);

	return true;
}

TEST_F(dirs, paths)
{
	PMEMfile *f;

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file", PMEMFILE_O_EXCL, 0644));

	f = pmemfile_open(pfp, "//file", 0);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_open(pfp, "/../file", 0);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_open(pfp, "/../../file", 0);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file"), 0) << strerror(errno);

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir////", 0755), 0) << strerror(errno);

	ASSERT_TRUE(list_files(pfp, "/", 3, 0, ". .. dir"));
	ASSERT_TRUE(list_files(pfp, "/dir", 2, 0, ". .."));

	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir//../dir/.//file",
					 PMEMFILE_O_EXCL, 0644));

	ASSERT_TRUE(list_files(pfp, "/dir", 3, 0, ". .. file"));

	f = pmemfile_open(pfp, "/dir/file", 0);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_open(pfp, "/dir/../dir////file", 0);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_open(pfp, "/dir/file/file", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOTDIR);

	f = pmemfile_open(pfp, "/dir/file/file",
			  PMEMFILE_O_RDONLY | PMEMFILE_O_CREAT, 0644);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOTDIR);

	f = pmemfile_open(pfp, "/dir/file/file", PMEMFILE_O_RDONLY |
				  PMEMFILE_O_CREAT | PMEMFILE_O_EXCL,
			  0644);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOTDIR);

	/* file is not a directory */
	errno = 0;
	f = pmemfile_open(pfp, "/dir/file/", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOTDIR);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir//file"), 0) << strerror(errno);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir//////"), 0) << strerror(errno);
}

TEST_F(dirs, lots_of_files)
{
	int ret;
	PMEMfile *f;
	char buf[1001];
	pmemfile_ssize_t written;

	ASSERT_TRUE(test_empty_dir(pfp, "/"));
	memset(buf, 0xff, sizeof(buf));

	for (size_t i = 0; i < ops; ++i) {
		sprintf(buf, "/file%04zu", i);

		f = pmemfile_open(pfp, buf, PMEMFILE_O_CREAT | PMEMFILE_O_EXCL |
					  PMEMFILE_O_WRONLY,
				  0644);
		ASSERT_NE(f, nullptr) << strerror(errno);

		written = pmemfile_write(pfp, f, buf, i);
		ASSERT_EQ(written, (pmemfile_ssize_t)i) << COND_ERROR(written);

		pmemfile_close(pfp, f);

		ASSERT_TRUE(list_files(pfp, "/", i + 1 + 2, 0,
				       "test1: after one iter"));
	}

	for (size_t i = 0; i < ops; ++i) {
		sprintf(buf, "/file%04zu", i);

		ret = pmemfile_unlink(pfp, buf);
		ASSERT_EQ(ret, 0) << strerror(errno);
	}

	/*
	 * For now pmemfile doesn't reclaim inode/dirent space when unlinking
	 * files and it's not easy to calculate how big an inode space is, so
	 * for now verify only when number of files is known.
	 */
	if (ops == 100)
		EXPECT_TRUE(test_compare_dirs(pfp, "/",
					      std::vector<pmemfile_ls>{
						      {040777, 2, 36864, "."},
						      {040777, 2, 36864, ".."},
					      }));
}

TEST_F(dirs, mkdir_rmdir_unlink_errors)
{
	char buf[1001];

	for (size_t i = 0; i < ops; ++i) {
		sprintf(buf, "/dir%04zu", i);

		ASSERT_EQ(pmemfile_mkdir(pfp, buf, 0755), 0);

		ASSERT_TRUE(list_files(pfp, "/", i + 1 + 2, 0,
				       "test2: after one iter"));
	}

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", 0755), -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_TRUE(list_files(pfp, "/", ops + 2, 1, "test2: after loop"));
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir0007/another_directory", 0755), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, NULL, 0755), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(NULL, "/dir", 0755), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", (pmemfile_mode_t)-1), -1);
	EXPECT_EQ(errno, EINVAL);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir0007", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2333/aaaa", 0755), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_TRUE(list_files(pfp, "/", ops + 2, 1, "test2: after2"));

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file", PMEMFILE_O_EXCL, 0644));

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/file/aaaa", 0755), -1);
	EXPECT_EQ(errno, ENOTDIR);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file"), 0);

	ASSERT_TRUE(list_files(pfp, "/", ops + 2, 1, "test2: after3"));

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, NULL), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(NULL, "/dir0000"), -1);
	EXPECT_EQ(errno, EFAULT);

	if (ops >= 100) {
		errno = 0;
		ASSERT_EQ(pmemfile_rmdir(pfp, "/dir0100"), -1);
		EXPECT_EQ(errno, ENOENT);
	}

	if (ops >= 99) {
		errno = 0;
		ASSERT_EQ(pmemfile_rmdir(pfp, "/dir0099/inside"), -1);
		EXPECT_EQ(errno, ENOENT);
	}

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file", PMEMFILE_O_EXCL, 0644));

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/file"), -1);
	EXPECT_EQ(errno, ENOTDIR);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdir(pfp, "/file/", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/file/"), -1);
	EXPECT_EQ(errno, ENOTDIR);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file"), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir0000"), -1);
	EXPECT_EQ(errno, EISDIR);

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir0007"), -1);
	EXPECT_EQ(errno, ENOTEMPTY);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir0007/another_directory"), 0);

	for (size_t i = 0; i < ops; ++i) {
		sprintf(buf, "/dir%04zu", i);

		ASSERT_EQ(pmemfile_rmdir(pfp, buf), 0);
	}

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/"), -1);
	EXPECT_EQ(errno, EBUSY);
}

TEST_F(dirs, read_write_dir)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_S_IRWXU), 0);

	PMEMfile *dir = pmemfile_open(pfp, "/dir",
				      PMEMFILE_O_DIRECTORY | PMEMFILE_O_RDONLY);
	ASSERT_NE(dir, nullptr);

	char buf[10];

	errno = 0;
	ASSERT_EQ(pmemfile_write(pfp, dir, buf, sizeof(buf)), -1);
	EXPECT_EQ(errno, EINVAL);

	errno = 0;
	ASSERT_EQ(pmemfile_read(pfp, dir, buf, sizeof(buf)), -1);
	EXPECT_EQ(errno, EISDIR);

	pmemfile_close(pfp, dir);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir"), 0);
}

TEST_F(dirs, mkdirat)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_S_IRWXU), 0);

	PMEMfile *dir = pmemfile_open(pfp, "/dir", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir, nullptr) << strerror(errno);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, NULL, 0755), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(NULL, dir, "internal", 0755), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "./", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "../dir", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "..", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "../dir/", 0755), -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_mkdirat(pfp, NULL, "internal", 0755), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "internal", PMEMFILE_S_IRWXU), 0);
	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "../external", PMEMFILE_S_IRWXU),
		  0);
	ASSERT_EQ(pmemfile_mkdirat(pfp, NULL, "/external3", PMEMFILE_S_IRWXU),
		  0);
	ASSERT_EQ(pmemfile_mkdirat(pfp, BADF, "/external4", PMEMFILE_S_IRWXU),
		  0);

	pmemfile_stat_t statbuf;
	ASSERT_EQ(pmemfile_stat(pfp, "/dir/internal", &statbuf), 0);
	ASSERT_EQ(PMEMFILE_S_ISDIR(statbuf.st_mode), 1);
	ASSERT_EQ(pmemfile_stat(pfp, "/external", &statbuf), 0);
	ASSERT_EQ(PMEMFILE_S_ISDIR(statbuf.st_mode), 1);

	ASSERT_EQ(pmemfile_chdir(pfp, "dir/internal"), 0);

	ASSERT_EQ(pmemfile_mkdirat(pfp, PMEMFILE_AT_CWD,
				   "dir-internal-internal", PMEMFILE_S_IRWXU),
		  0);
	ASSERT_EQ(pmemfile_mkdirat(pfp, PMEMFILE_AT_CWD, "../dir-internal2",
				   PMEMFILE_S_IRWXU),
		  0);
	ASSERT_EQ(pmemfile_mkdirat(pfp, PMEMFILE_AT_CWD, "../../external2",
				   PMEMFILE_S_IRWXU),
		  0);

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_mkdirat(pfp, PMEMFILE_AT_CWD,
					   "dir-fault-inject",
					   PMEMFILE_S_IRWXU),
			  -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_chdir(pfp, "../.."), 0);

	pmemfile_close(pfp, dir);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir/internal/dir-internal-internal"),
		  0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir/dir-internal2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir/internal"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/external"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/external2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/external3"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/external4"), 0);
}

TEST_F(dirs, unlinkat)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_S_IRWXU), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir/internal", PMEMFILE_S_IRWXU), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file1", PMEMFILE_O_EXCL, 0644));

	PMEMfile *dir = pmemfile_open(pfp, "/dir", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir, nullptr) << strerror(errno);

	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir/file", PMEMFILE_O_EXCL, 0644));
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir/file2", PMEMFILE_O_EXCL, 0644));
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir/file3", PMEMFILE_O_EXCL, 0644));

	errno = 0;
	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, NULL, 0), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_unlinkat(NULL, dir, "file", 0), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_unlinkat(pfp, NULL, "file", 0), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_unlinkat(pfp, NULL, "/dir/file2", 0), 0);

	ASSERT_EQ(pmemfile_unlinkat(pfp, BADF, "/dir/file3", 0), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "file", ~PMEMFILE_AT_REMOVEDIR),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "file", 0), -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "file", 0), 0);
	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "../file1", 0), 0);

	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "internal", 0), -1);
	EXPECT_EQ(errno, EISDIR);

	ASSERT_EQ(
		pmemfile_unlinkat(pfp, dir, "internal", PMEMFILE_AT_REMOVEDIR),
		0);

	pmemfile_close(pfp, dir);
	ASSERT_EQ(pmemfile_unlinkat(pfp, PMEMFILE_AT_CWD, "dir", 0), -1);
	EXPECT_EQ(errno, EISDIR);

	ASSERT_EQ(pmemfile_unlinkat(pfp, PMEMFILE_AT_CWD, "dir",
				    PMEMFILE_AT_REMOVEDIR),
		  0);
}

TEST_F(dirs, rmdir_notempty)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir1/file", PMEMFILE_O_EXCL, 0644));

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), -1);
	ASSERT_EQ(errno, ENOTEMPTY);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file"), 0);

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2", 0755), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), -1);
	ASSERT_EQ(errno, ENOTEMPTY);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2"), 0);

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, rmdir_notempty_dir_with_holes)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);

	char buf[1001];
	for (size_t i = 0; i < ops; ++i) {
		sprintf(buf, "/dir1/file%04zu", i);
		ASSERT_TRUE(
			test_pmemfile_create(pfp, buf, PMEMFILE_O_EXCL, 0644));
	}

	for (size_t i = 0; i < ops / 2; ++i) {
		sprintf(buf, "/dir1/file%04zu", i);
		ASSERT_EQ(pmemfile_unlink(pfp, buf), 0);
	}

	errno = 0;
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), -1);
	ASSERT_EQ(errno, ENOTEMPTY);

	for (size_t i = ops / 2; i < ops; ++i) {
		sprintf(buf, "/dir1/file%04zu", i);
		ASSERT_EQ(pmemfile_unlink(pfp, buf), 0);
	}

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, chdir_getcwd)
{
	char buf[PMEMFILE_PATH_MAX];

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2/dir3", 0755), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_getcwd(NULL, buf, sizeof(buf)), nullptr);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, NULL), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_chdir(NULL, "/dir1"), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_chdir(pfp, "/dir1"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1");

	ASSERT_EQ(pmemfile_chdir(pfp, "/dir1/dir2"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2");

	ASSERT_EQ(pmemfile_chdir(pfp, "/dir1/dir2/dir3"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2/dir3");

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2");

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1");

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, "dir1/.."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, "dir1"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1");

	ASSERT_EQ(pmemfile_chdir(pfp, "dir2"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2");

	ASSERT_EQ(pmemfile_chdir(pfp, "dir3"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2/dir3");

	ASSERT_EQ(pmemfile_chdir(pfp, "."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/dir1/dir2/dir3");

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2/dir3"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);

	errno = 0;
	ASSERT_EQ(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_EQ(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_EQ(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, "."), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	ASSERT_EQ(pmemfile_chdir(pfp, "./././././"), 0);
	ASSERT_NE(pmemfile_getcwd(pfp, buf, sizeof(buf)), nullptr);
	ASSERT_STREQ(buf, "/");

	errno = 0;
	ASSERT_EQ(pmemfile_chdir(pfp, "dir1/../"), -1);
	EXPECT_EQ(errno, ENOENT);

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		errno = 0;
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		ASSERT_EQ(pmemfile_chdir(pfp, "."), -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file", 0, 0777));
	errno = 0;
	ASSERT_EQ(pmemfile_chdir(pfp, "file"), -1);
	EXPECT_EQ(errno, ENOTDIR);

	errno = 0;
	ASSERT_EQ(pmemfile_chdir(pfp, "file/file"), -1);
	EXPECT_EQ(errno, ENOTDIR);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file"), 0);

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	PMEMfile *f = pmemfile_open(pfp, "dir1", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(f, nullptr) << strerror(errno);

	ASSERT_EQ(pmemfile_fchdir(pfp, NULL), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_fchdir(NULL, f), -1);
	EXPECT_EQ(errno, EFAULT);

	if (_pmemfile_fault_injection_enabled()) {
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_fchdir(pfp, f), -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_fchdir(pfp, f), 0);

	pmemfile_close(pfp, f);

	errno = 0;
	ASSERT_EQ(pmemfile_getcwd(pfp, buf, 0), nullptr);
	EXPECT_EQ(errno, EINVAL);

	char *t;

	t = pmemfile_getcwd(pfp, NULL, 0);
	ASSERT_NE(t, nullptr);
	ASSERT_STREQ(t, "/dir1");
	free(t);

	t = pmemfile_getcwd(pfp, NULL, 10);
	ASSERT_NE(t, nullptr);
	ASSERT_STREQ(t, "/dir1");
	free(t);

	t = pmemfile_getcwd(pfp, NULL, 2);
	ASSERT_EQ(t, nullptr);
	EXPECT_EQ(errno, ERANGE);

	for (size_t i = 1; i < strlen("/dir1") + 1; ++i) {
		errno = 0;
		ASSERT_EQ(pmemfile_getcwd(pfp, buf, i), nullptr);
		EXPECT_EQ(errno, ERANGE);
	}
	ASSERT_NE(pmemfile_getcwd(pfp, buf, strlen("/dir1") + 1), nullptr);
	ASSERT_STREQ(buf, "/dir1");

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, get_dir_path)
{
	char buf[100];

	errno = 0;
	ASSERT_EQ(
		pmemfile_get_dir_path(NULL, PMEMFILE_AT_CWD, buf, sizeof(buf)),
		nullptr);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_get_dir_path(pfp, NULL, buf, sizeof(buf)), nullptr);
	EXPECT_EQ(errno, EFAULT);

	if (_pmemfile_fault_injection_enabled()) {
		_pmemfile_inject_fault_at(PF_MALLOC, 1,
					  "_pmemfile_get_dir_path");

		errno = 0;
		ASSERT_EQ(pmemfile_get_dir_path(pfp, PMEMFILE_AT_CWD, NULL, 0),
			  nullptr);
		EXPECT_EQ(errno, ENOMEM);
	}
}

TEST_F(dirs, relative_paths)
{
	pmemfile_stat_t stat;

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_chdir(pfp, "/dir1"), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "../file1", 0, 0755));
	ASSERT_TRUE(test_pmemfile_create(pfp, "file2", 0, 0755));
	ASSERT_EQ(pmemfile_unlink(pfp, "file2"), 0);
	ASSERT_EQ(pmemfile_link(pfp, "../file1", "file2"), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "file2", &stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "../file1", &stat), 0);
	ASSERT_EQ(pmemfile_lstat(pfp, "file2", &stat), 0);
	ASSERT_EQ(pmemfile_lstat(pfp, "../file1", &stat), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "../dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "dir3", 0755), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir2", &stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/dir3", &stat), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir3/.."), -1);
	EXPECT_EQ(errno, ENOTEMPTY);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir3/."), -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/file2/file"), -1);
	EXPECT_EQ(errno, ENOTDIR);

	ASSERT_EQ(pmemfile_rmdir(pfp, "../dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "dir3"), 0);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file2"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file1"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
	ASSERT_EQ(pmemfile_chdir(pfp, "/"), 0);
}

TEST_F(dirs, file_renames)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", 0755), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/file1", 0, 0755));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir2/file2", 0, 0755));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/file3", 0, 0755));

	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 4, 8192, "."},
						    {040777, 4, 8192, ".."},
						    {040755, 2, 8192, "dir1"},
						    {040755, 2, 8192, "dir2"},
						    {0100755, 1, 0, "file3"},
					    }));
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file1"},
				      }));
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file2"},
				      }));

	errno = 0;
	ASSERT_EQ(pmemfile_rename(pfp, "/file3", NULL), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_rename(pfp, NULL, "/file4"), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_rename(NULL, "/file3", "/file4"), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_rename(pfp, "/file3", "file4"), 0);
	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 4, 8192, "."},
						    {040777, 4, 8192, ".."},
						    {040755, 2, 8192, "dir1"},
						    {040755, 2, 8192, "dir2"},
						    {0100755, 1, 0, "file4"},
					    }));
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/file1", "/dir1/file11"), 0);
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file11"},
				      }));
	ASSERT_EQ(pmemfile_rename(pfp, "/dir2/file2", "/dir2/file22"), 0);
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file22"},
				      }));

	ASSERT_EQ(pmemfile_rename(pfp, "/file4", "/dir2/file4"), 0);
	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 4, 8192, "."},
						    {040777, 4, 8192, ".."},
						    {040755, 2, 8192, "dir1"},
						    {040755, 2, 8192, "dir2"},
					    }));
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file4"},
					      {0100755, 1, 0, "file22"},
				      }));
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/file11", "/dir2/file11"), 0);
	EXPECT_TRUE(
		test_compare_dirs(pfp, "/dir1", std::vector<pmemfile_ls>{
							{040755, 2, 8192, "."},
							{040777, 4, 8192, ".."},
						}));
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file4"},
					      {0100755, 1, 0, "file22"},
					      {0100755, 1, 0, "file11"},
				      }));
	ASSERT_EQ(pmemfile_rename(pfp, "/dir2/file11", "/dir2/file22"), 0);
	EXPECT_TRUE(test_compare_dirs(pfp, "/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100755, 1, 0, "file4"},
					      {0100755, 1, 0, "file22"},
				      }));

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_rename(pfp, "/dir2/file22", "/dir2/file11"),
			  -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file22"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file4"), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);

	/*
	 * From "rename" manpage:
	 * "If oldpath and newpath are existing hard links referring to
	 * the same file, then rename() does nothing, and returns a success
	 * status."
	 */
	ASSERT_TRUE(test_pmemfile_create(pfp, "/file1", 0, 0755));
	ASSERT_EQ(pmemfile_link(pfp, "/file1", "/file2"), 0);
	ASSERT_EQ(pmemfile_rename(pfp, "/file1", "/file2"), 0);
	pmemfile_stat_t stat_buf;
	ASSERT_EQ(pmemfile_stat(pfp, "/file1", &stat_buf), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/file2", &stat_buf), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file2"), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/"), -1);
	EXPECT_EQ(errno, EBUSY);
}

TEST_F(dirs, file_renames_lock_files_in_different_order)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", 0755), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/file1", 0, 0755));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/file2", 0, 0755));

	/*
	 * We have to keep these files open for duration of the test, to not
	 * let pmemfile free the underlying vinodes, so that helgrind can notice
	 * we are locking them in wrong order.
	 */
	PMEMfile *f1 = pmemfile_open(pfp, "/file1", PMEMFILE_O_RDONLY);
	PMEMfile *f2 = pmemfile_open(pfp, "/file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f1, nullptr);
	ASSERT_NE(f2, nullptr);

	ASSERT_EQ(pmemfile_link(pfp, "/file1", "/dir1/file1"), 0);
	ASSERT_EQ(pmemfile_link(pfp, "/file2", "/dir2/file2"), 0);

	/* file1 -> file2 */
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/file1", "/dir2/file2"), 0);

	/* restore the initial situation */
	ASSERT_EQ(pmemfile_link(pfp, "/file1", "/dir1/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file2"), 0);
	ASSERT_EQ(pmemfile_link(pfp, "/file2", "/dir2/file2"), 0);

	/* file2 -> file1 */
	ASSERT_EQ(pmemfile_rename(pfp, "/dir2/file2", "/dir1/file1"), 0);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file2"), 0);

	pmemfile_close(pfp, f1);
	pmemfile_close(pfp, f2);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, file_renames_in_same_tree)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2/dir3", 0755), 0);
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/dir2/dir3/file", 0, 0755));
	ASSERT_EQ(
		pmemfile_rename(pfp, "/dir1/dir2/dir3/file", "/dir1/dir2/file"),
		0);
	ASSERT_EQ(
		pmemfile_rename(pfp, "/dir1/dir2/file", "/dir1/dir2/dir3/file"),
		0);
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/dir2/dir3/file", "/dir1/file"),
		  0);
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/file", "/dir1/dir2/dir3/file"),
		  0);
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/dir2/dir3/file", "/file"), 0);
	ASSERT_EQ(pmemfile_rename(pfp, "/file", "/dir1/file"), 0);
	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/file", "/dir1/dir2/file"), 0);
	ASSERT_EQ(
		pmemfile_rename(pfp, "/dir1/dir2/file", "/dir1/dir2/dir3/file"),
		0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/dir2/dir3/file"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2/dir3"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

static bool
same_inode(PMEMfilepool *pfp, const char *path, const pmemfile_stat_t *tmpl)
{
	pmemfile_stat_t buf;
	if (pmemfile_stat(pfp, path, &buf)) {
		ADD_FAILURE() << strerror(errno);
		return false;
	}

	if (buf.st_dev != tmpl->st_dev || buf.st_ino != tmpl->st_ino) {
		ADD_FAILURE() << "(" << buf.st_dev << ", " << buf.st_ino
			      << ") != (" << tmpl->st_dev << ", "
			      << tmpl->st_ino << ")";
		return false;
	}

	return true;
}

TEST_F(dirs, rename_dir)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2/dir3", 0755), 0);
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/f1", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/dir2/f2", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/dir2/dir3/f3", 0, 0644));

	pmemfile_stat_t root_stat, dir1_stat, dir2_stat, dir3_stat;

	ASSERT_EQ(pmemfile_stat(pfp, "/", &root_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1", &dir1_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/dir2", &dir2_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/dir2/dir3", &dir3_stat), 0);

	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 3, 8192, "."},
						    {040777, 3, 8192, ".."},
						    {040755, 3, 8192, "dir1"},
					    }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040777, 3, 8192, ".."},
					      {040755, 3, 8192, "dir2"},
					      {0100644, 1, 0, "f1"},
				      }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040755, 3, 8192, ".."},
					      {040755, 2, 8192, "dir3"},
					      {0100644, 1, 0, "f2"},
				      }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1/dir2/dir3",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040755, 3, 8192, ".."},
					      {0100644, 1, 0, "f3"},
				      }));

	EXPECT_TRUE(same_inode(pfp, "/dir1/dir2/dir3/..", &dir2_stat));

	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/dir2/dir3", "/dir1/dir2/dir31"),
		  0)
		<< strerror(errno);

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1/dir2",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040755, 3, 8192, ".."},
					      {040755, 2, 8192, "dir31"},
					      {0100644, 1, 0, "f2"},
				      }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1/dir2/dir31",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040755, 3, 8192, ".."},
					      {0100644, 1, 0, "f3"},
				      }));

	EXPECT_TRUE(same_inode(pfp, "/dir1/dir2", &dir2_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir2/dir31", &dir3_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir2/dir31/.", &dir3_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir2/dir31/..", &dir2_stat));

	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/dir2", "/dir1/dir21"), 0)
		<< strerror(errno);

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040777, 3, 8192, ".."},
					      {040755, 3, 8192, "dir21"},
					      {0100644, 1, 0, "f1"},
				      }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1/dir21",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040755, 3, 8192, ".."},
					      {040755, 2, 8192, "dir31"},
					      {0100644, 1, 0, "f2"},
				      }));

	EXPECT_TRUE(same_inode(pfp, "/dir1/dir21", &dir2_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir21/.", &dir2_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir21/..", &dir1_stat));

	ASSERT_EQ(pmemfile_rename(pfp, "/dir1", "/dir11"), 0)
		<< strerror(errno);

	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 3, 8192, "."},
						    {040777, 3, 8192, ".."},
						    {040755, 3, 8192, "dir11"},
					    }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir11",
				      std::vector<pmemfile_ls>{
					      {040755, 3, 8192, "."},
					      {040777, 3, 8192, ".."},
					      {040755, 3, 8192, "dir21"},
					      {0100644, 1, 0, "f1"},
				      }));

	EXPECT_TRUE(same_inode(pfp, "/dir11", &dir1_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir11/.", &dir1_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir11/..", &root_stat));

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir11/dir21/dir31/f3"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir11/dir21/f2"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir11/f1"), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir11/dir21/dir31"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir11/dir21"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir11"), 0);
}

TEST_F(dirs, move_dirs_between_dirs)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1/dir2/dir3", 0755), 0);
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/f1", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/dir2/f2", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/dir2/dir3/f3", 0, 0644));

	pmemfile_stat_t root_stat, dir1_stat, dir2_stat, dir3_stat;

	ASSERT_EQ(pmemfile_stat(pfp, "/", &root_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1", &dir1_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/dir2", &dir2_stat), 0);
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/dir2/dir3", &dir3_stat), 0);

	ASSERT_EQ(pmemfile_rename(pfp, "/dir1", "/dir1/dir2/dirX"), -1);
	EXPECT_EQ(errno, EINVAL);

	EXPECT_TRUE(
		test_compare_dirs(pfp, "/", std::vector<pmemfile_ls>{
						    {040777, 3, 8192, "."},
						    {040777, 3, 8192, ".."},
						    {040755, 3, 8192, "dir1"},
					    }));

	ASSERT_EQ(pmemfile_rename(pfp, "/dir1/dir2/dir3", "/dir1/dir3"), 0)
		<< strerror(errno);

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir1",
				      std::vector<pmemfile_ls>{
					      {040755, 4, 8192, "."},
					      {040777, 3, 8192, ".."},
					      {040755, 2, 8192, "dir2"},
					      {0100644, 1, 0, "f1"},
					      {040755, 2, 8192, "dir3"},
				      }));

	EXPECT_TRUE(same_inode(pfp, "/dir1/dir3", &dir3_stat));
	EXPECT_TRUE(same_inode(pfp, "/dir1/dir3/..", &dir1_stat));

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/dir3/f3"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/dir2/f2"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/f1"), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir3"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, rename_dir_to_empty)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir_empty", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir_not_empty", 0755), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir/f1", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir_not_empty/f2", 0, 0644));

	ASSERT_EQ(pmemfile_rename(pfp, "/dir", "/dir_not_empty"), -1);
	EXPECT_EQ(errno, ENOTEMPTY); // or EEXIST

	ASSERT_EQ(pmemfile_rename(pfp, "/dir", "/dir_empty"), 0)
		<< strerror(errno);

	EXPECT_TRUE(test_compare_dirs(
		pfp, "/", std::vector<pmemfile_ls>{
				  {040777, 4, 8192, "."},
				  {040777, 4, 8192, ".."},
				  {040755, 2, 8192, "dir_empty"},
				  {040755, 2, 8192, "dir_not_empty"},
			  }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir_empty",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100644, 1, 0, "f1"},
				      }));

	EXPECT_TRUE(test_compare_dirs(pfp, "/dir_not_empty",
				      std::vector<pmemfile_ls>{
					      {040755, 2, 8192, "."},
					      {040777, 4, 8192, ".."},
					      {0100644, 1, 0, "f2"},
				      }));
}

TEST_F(dirs, renameat)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", 0755), 0);
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/f1", 0, 0644));

	PMEMfile *dir1 = pmemfile_open(pfp, "/dir1", PMEMFILE_O_DIRECTORY);
	PMEMfile *dir2 = pmemfile_open(pfp, "/dir2", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir1, nullptr);
	ASSERT_NE(dir2, nullptr);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat(pfp, PMEMFILE_AT_CWD, NULL, PMEMFILE_AT_CWD,
				    "ff"),
		  -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat(pfp, PMEMFILE_AT_CWD, "f1", PMEMFILE_AT_CWD,
				    NULL),
		  -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat(pfp, NULL, "f1", PMEMFILE_AT_CWD, "ff"),
		  -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat(pfp, PMEMFILE_AT_CWD, "f1", NULL, "ff"),
		  -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat(NULL, PMEMFILE_AT_CWD, "f1",
				    PMEMFILE_AT_CWD, "ff"),
		  -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_renameat(pfp, dir1, "f1", dir2, "ff"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_renameat(pfp, NULL, "/dir2/ff", dir1, "f1"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_renameat(pfp, BADF, "/dir1/f1", dir2, "ff"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_renameat(pfp, dir2, "ff", NULL, "/dir1/f1"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_renameat(pfp, dir1, "f1", BADF, "/dir2/ff"), 0)
		<< strerror(errno);

	pmemfile_stat_t buf;
	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/f1", &buf), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_stat(pfp, "/dir2/ff", &buf), 0) << strerror(errno);

	ASSERT_EQ(pmemfile_fchdir(pfp, dir2), 0);

	ASSERT_EQ(pmemfile_renameat(pfp, PMEMFILE_AT_CWD, "ff", dir1, "f2"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_stat(pfp, "/dir2/ff", &buf), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/f2", &buf), 0) << strerror(errno);

	ASSERT_EQ(pmemfile_renameat(pfp, dir1, "f2", PMEMFILE_AT_CWD, "f3"), 0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_stat(pfp, "/dir1/f2", &buf), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_stat(pfp, "/dir2/f3", &buf), 0) << strerror(errno);

	pmemfile_close(pfp, dir1);
	pmemfile_close(pfp, dir2);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/f3"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

TEST_F(dirs, rename_noreplace)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", 0755), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/f1", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir2/f2", 0, 0644));

	errno = 0;
	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir1/f1", NULL, "/dir2/f2",
				     PMEMFILE_RENAME_WHITEOUT),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir1/f1", NULL, "/dir2/f2",
				     PMEMFILE_RENAME_NOREPLACE |
					     PMEMFILE_RENAME_EXCHANGE),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir1/f1", NULL, "/dir2/f2",
				     ~((unsigned)(PMEMFILE_RENAME_EXCHANGE |
						  PMEMFILE_RENAME_NOREPLACE |
						  PMEMFILE_RENAME_WHITEOUT))),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir1/f1", NULL, "/dir2/f2",
				     PMEMFILE_RENAME_NOREPLACE),
		  -1);
	EXPECT_EQ(errno, EEXIST);

	ASSERT_EQ(
		pmemfile_renameat2(pfp, NULL, "/dir1/f1", NULL, "/dir2/f2", 0),
		0)
		<< strerror(errno);

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2", NULL, "/dir1",
				     PMEMFILE_RENAME_NOREPLACE),
		  -1);
	EXPECT_EQ(errno, EEXIST);

	errno = 0;
	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2", NULL, "/dir1",
				     0xFFFFFFFF),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2", NULL, "/dir1", 0), 0);

	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/f2"), 0);

	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

static bool
validate_nlink(PMEMfilepool *pfp, const char *path, pmemfile_nlink_t nlink)
{
	pmemfile_stat_t buf;
	if (pmemfile_lstat(pfp, path, &buf)) {
		ADD_FAILURE() << strerror(errno);
		return false;
	}

	EXPECT_EQ(nlink, buf.st_nlink);
	if (nlink != buf.st_nlink)
		return false;

	return true;
}

TEST_F(dirs, rename_exchange)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", 0755), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2/dir3", 0755), 0);
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir1/f1", 0, 0644));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir2/dir3/f2", 0, 0644));
	ASSERT_EQ(pmemfile_symlink(pfp, "/dir1", "/dirX"), 0);

	/*
	 * Now:
	 *  / is a dir (links: . .. /dir1/.. /dir2/..)
	 *  /dir1 is a dir (links: /dir1 /dir1/.)
	 *  /dir2 is a dir (links: /dir2 /dir2/. /dir2/dir3/..)
	 *  /dir2/dir3 is a dir (links: /dir2/dir3 /dir2/dir3/.)
	 *  /dirX is a symlink to /dir1 (links: /dirX)
	 */

	ASSERT_TRUE(validate_nlink(pfp, "/", 4));
	ASSERT_TRUE(validate_nlink(pfp, "/dir1/.", 2));
	ASSERT_TRUE(validate_nlink(pfp, "/dir2/.", 3));
	ASSERT_TRUE(validate_nlink(pfp, "/dir2/dir3/.", 2));
	ASSERT_TRUE(validate_nlink(pfp, "/dirX/.", 2));

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2/dir3", NULL, "/dirX",
				     PMEMFILE_RENAME_EXCHANGE),
		  0)
		<< strerror(errno);

	/*
	 * Now:
	 *  / is a dir (links: . .. /dir1/.. /dir2/.. /dirX/..)
	 *  /dir1 is a dir (links: /dir1 /dir1/.)
	 *  /dir2 is a dir (links: /dir2 /dir2/.)
	 *  /dir2/dir3 is a symlink to /dir1 (links: /dir2/dir3)
	 *  /dirX is a dir (links: /dirX /dirX/.)
	 */

	pmemfile_stat_t buf;
	ASSERT_EQ(pmemfile_lstat(pfp, "/dir2/dir3", &buf), 0);
	ASSERT_TRUE(PMEMFILE_S_ISLNK(buf.st_mode));
	ASSERT_EQ(pmemfile_lstat(pfp, "/dirX", &buf), 0);
	ASSERT_TRUE(PMEMFILE_S_ISDIR(buf.st_mode));

	ASSERT_EQ(pmemfile_stat(pfp, "/dir2/dir3/f2", &buf), -1);
	EXPECT_EQ(errno, ENOENT);
	ASSERT_EQ(pmemfile_stat(pfp, "/dirX/f2", &buf), 0);

	ASSERT_TRUE(validate_nlink(pfp, "/", 5));
	ASSERT_TRUE(validate_nlink(pfp, "/dir1/.", 2));
	ASSERT_TRUE(validate_nlink(pfp, "/dir2/.", 2));
	ASSERT_TRUE(validate_nlink(pfp, "/dir2/dir3/.", 2));
	ASSERT_TRUE(validate_nlink(pfp, "/dirX/.", 2));

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2", NULL, "/dir2/dir3",
				     PMEMFILE_RENAME_EXCHANGE),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2/dir3", NULL, "/dir2",
				     PMEMFILE_RENAME_EXCHANGE),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_renameat2(pfp, NULL, "/dir2", NULL, "/not_existing",
				     PMEMFILE_RENAME_EXCHANGE),
		  -1);
	EXPECT_EQ(errno, ENOENT);
}

static bool
is_owned(PMEMfilepool *pfp, const char *path, pmemfile_uid_t owner)
{
	pmemfile_stat_t st;
	memset(&st, 0xff, sizeof(st));

	int r = pmemfile_lstat(pfp, path, &st);
	EXPECT_EQ(r, 0) << strerror(errno);
	if (r)
		return false;

	EXPECT_EQ(st.st_uid, owner);
	if (st.st_uid != owner)
		return false;

	return true;
}

TEST_F(dirs, fchownat)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_ACCESSPERMS), 0);
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir/file1", 0, PMEMFILE_S_IRWXU));
	ASSERT_EQ(pmemfile_symlink(pfp, "/dir/file1", "/symlink"), 0);

	PMEMfile *dir = pmemfile_open(pfp, "/dir", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir, nullptr) << strerror(errno);

	ASSERT_EQ(pmemfile_fchownat(pfp, PMEMFILE_AT_CWD, NULL, 0, 0, 0), -1);
	EXPECT_EQ(errno, ENOENT);

	ASSERT_EQ(pmemfile_fchownat(pfp, NULL, "dir", 0, 0, 0), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_fchownat(NULL, PMEMFILE_AT_CWD, "dir", 0, 0, 0), -1);
	EXPECT_EQ(errno, EFAULT);

	ASSERT_EQ(pmemfile_fchownat(pfp, PMEMFILE_AT_CWD, "dir", 0, 0,
				    ~(PMEMFILE_AT_EMPTY_PATH |
				      PMEMFILE_AT_SYMLINK_NOFOLLOW)),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_setuid(pfp, 1000), 0);
	ASSERT_EQ(pmemfile_setcap(pfp, PMEMFILE_CAP_CHOWN), 0)
		<< strerror(errno);

	ASSERT_TRUE(is_owned(pfp, "/dir", 0));
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 0));

	ASSERT_EQ(pmemfile_fchownat(pfp, PMEMFILE_AT_CWD, "dir", 2000, 2000, 0),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/dir", 2000));

	ASSERT_EQ(pmemfile_fchownat(pfp, dir, "", 1000, 1000, 0), -1);
	EXPECT_EQ(errno, ENOENT);
	ASSERT_TRUE(is_owned(pfp, "/dir", 2000));

	ASSERT_EQ(pmemfile_fchownat(pfp, dir, "", 1000, 1000,
				    PMEMFILE_AT_EMPTY_PATH),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/dir", 1000));

	ASSERT_EQ(pmemfile_fchownat(pfp, dir, "file1", 1000, 1000, 0), 0);
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 1000));

	ASSERT_EQ(pmemfile_fchownat(pfp, PMEMFILE_AT_CWD, "symlink", 1001, 1001,
				    0),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/symlink", 0));
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 1001));

	ASSERT_EQ(pmemfile_fchownat(pfp, PMEMFILE_AT_CWD, "symlink", 1002, 1002,
				    PMEMFILE_AT_SYMLINK_NOFOLLOW),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/symlink", 1002));
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 1001));

	ASSERT_EQ(pmemfile_fchownat(pfp, NULL, "/symlink", 1003, 1003,
				    PMEMFILE_AT_SYMLINK_NOFOLLOW),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/symlink", 1003));
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 1001));

	ASSERT_EQ(pmemfile_fchownat(pfp, BADF, "/symlink", 1004, 1004,
				    PMEMFILE_AT_SYMLINK_NOFOLLOW),
		  0);
	ASSERT_TRUE(is_owned(pfp, "/symlink", 1004));
	ASSERT_TRUE(is_owned(pfp, "/dir/file1", 1001));

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_fchownat(pfp, dir, "file1", 1000, 1000, 0),
			  -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	ASSERT_EQ(pmemfile_clrcap(pfp, PMEMFILE_CAP_CHOWN), 0)
		<< strerror(errno);

	pmemfile_close(pfp, dir);

	ASSERT_EQ(pmemfile_unlink(pfp, "/symlink"), 0) << strerror(errno);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir/file1"), 0) << strerror(errno);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir"), 0);
}

TEST_F(dirs, openat)
{
	PMEMfile *dir, *f;

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_S_IRWXU), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", PMEMFILE_S_IRWXU), 0);
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir/file1", 0, PMEMFILE_S_IRWXU));
	ASSERT_TRUE(test_pmemfile_create(pfp, "/file2", 0, PMEMFILE_S_IRWXU));

	dir = pmemfile_open(pfp, "/dir", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir, nullptr) << strerror(errno);

	f = pmemfile_openat(NULL, dir, "file1", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, EFAULT);

	f = pmemfile_openat(pfp, NULL, "file1", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, EFAULT);

	f = pmemfile_openat(pfp, dir, "file1", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, dir, "file2", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOENT);

	f = pmemfile_openat(pfp, dir, "../file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, NULL, "/file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, BADF, "/file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "file1", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOENT);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "dir/file1",
			    PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	ASSERT_EQ(pmemfile_chdir(pfp, "dir2"), 0);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "file1", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOENT);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "dir/file1",
			    PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOENT);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "file2", PMEMFILE_O_RDONLY);
	ASSERT_EQ(f, nullptr);
	EXPECT_EQ(errno, ENOENT);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "/dir/file1",
			    PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	f = pmemfile_openat(pfp, PMEMFILE_AT_CWD, "/file2", PMEMFILE_O_RDONLY);
	ASSERT_NE(f, nullptr) << strerror(errno);
	pmemfile_close(pfp, f);

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_FALSE(pmemfile_openat(pfp, PMEMFILE_AT_CWD, "/file2",
					     PMEMFILE_O_RDONLY));
		EXPECT_EQ(errno, ENOMEM);
	}

	pmemfile_close(pfp, dir);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file2"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir/file1"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir2"), 0);
}

static bool
test_file_info(PMEMfilepool *pfp, const char *path, pmemfile_nlink_t nlink,
	       pmemfile_ino_t ino)
{
	pmemfile_stat_t st;
	memset(&st, 0, sizeof(st));

	int r = pmemfile_lstat(pfp, path, &st);

	EXPECT_EQ(r, 0) << strerror(errno);
	if (r)
		return false;

	EXPECT_EQ(st.st_nlink, nlink);
	EXPECT_EQ(st.st_ino, ino);
	if (st.st_nlink != nlink || st.st_ino != ino)
		return false;

	return true;
}

TEST_F(dirs, linkat)
{
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir1", PMEMFILE_S_IRWXU), 0);
	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir2", PMEMFILE_S_IRWXU), 0);

	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir1/file1", 0, PMEMFILE_S_IRWXU));
	ASSERT_TRUE(
		test_pmemfile_create(pfp, "/dir2/file2", 0, PMEMFILE_S_IRWXU));

	pmemfile_stat_t st_file1, st_file2, st_file1_sym;
	ASSERT_EQ(pmemfile_lstat(pfp, "/dir1/file1", &st_file1), 0);
	ASSERT_EQ(pmemfile_lstat(pfp, "/dir2/file2", &st_file2), 0);

	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1", 1, st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir2/file2", 1, st_file2.st_ino));

	ASSERT_EQ(pmemfile_symlink(pfp, "/dir1/file1", "/dir2/file1-sym"), 0);

	ASSERT_EQ(pmemfile_lstat(pfp, "/dir2/file1-sym", &st_file1_sym), 0);

	PMEMfile *dir1 = pmemfile_open(pfp, "/dir1", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir1, nullptr) << strerror(errno);

	PMEMfile *dir2 = pmemfile_open(pfp, "/dir2", PMEMFILE_O_DIRECTORY);
	ASSERT_NE(dir2, nullptr) << strerror(errno);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(pfp, dir1, NULL, dir2, "file1", 0), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "file1", dir2, NULL, 0), -1);
	EXPECT_EQ(errno, ENOENT);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(pfp, NULL, "file1", dir2, "file1", 0), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "file1", NULL, "file1", 0), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(NULL, dir1, "file1", dir2, "file1", 0), -1);
	EXPECT_EQ(errno, EFAULT);

	errno = 0;
	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "file1", dir2, "file1",
				  ~(PMEMFILE_AT_SYMLINK_FOLLOW |
				    PMEMFILE_AT_EMPTY_PATH)),
		  -1);
	EXPECT_EQ(errno, EINVAL);

	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "file1", dir2, "file1", 0), 0);
	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1", 2, st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir2/file1", 2, st_file1.st_ino));

	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "file1", PMEMFILE_AT_CWD, "file1",
				  0),
		  0);
	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1", 3, st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir2/file1", 3, st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/file1", 3, st_file1.st_ino));

	/* both paths are relative to cwd */
	ASSERT_EQ(pmemfile_linkat(pfp, PMEMFILE_AT_CWD, "dir1/file1",
				  PMEMFILE_AT_CWD, "file11", 0),
		  0);
	ASSERT_EQ(pmemfile_unlink(pfp, "file11"), 0);

	ASSERT_EQ(pmemfile_linkat(pfp, NULL, "/dir1/file1", PMEMFILE_AT_CWD,
				  "file11", 0),
		  0);
	ASSERT_EQ(pmemfile_unlink(pfp, "file11"), 0);

	ASSERT_EQ(pmemfile_linkat(pfp, BADF, "/dir1/file1", PMEMFILE_AT_CWD,
				  "file11", 0),
		  0);
	ASSERT_EQ(pmemfile_unlink(pfp, "file11"), 0);

	ASSERT_EQ(pmemfile_linkat(pfp, PMEMFILE_AT_CWD, "dir1/file1", NULL,
				  "/file11", 0),
		  0);
	ASSERT_EQ(pmemfile_unlink(pfp, "file11"), 0);

	ASSERT_EQ(pmemfile_linkat(pfp, PMEMFILE_AT_CWD, "dir1/file1", BADF,
				  "/file11", 0),
		  0);
	ASSERT_EQ(pmemfile_unlink(pfp, "file11"), 0);

	ASSERT_TRUE(
		test_file_info(pfp, "/dir2/file1-sym", 1, st_file1_sym.st_ino));
	ASSERT_EQ(pmemfile_linkat(pfp, dir2, "file1-sym", dir1,
				  "file1-link-to-symlink", 0),
		  0);
	ASSERT_TRUE(
		test_file_info(pfp, "/dir2/file1-sym", 2, st_file1_sym.st_ino));

	ASSERT_EQ(pmemfile_linkat(pfp, dir2, "file1-sym", dir1,
				  "file1-link-to-deref-symlink",
				  PMEMFILE_AT_SYMLINK_FOLLOW),
		  0);
	ASSERT_TRUE(
		test_file_info(pfp, "/dir2/file1-sym", 2, st_file1_sym.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1-link-to-deref-symlink", 4,
				   st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1", 4, st_file1.st_ino));

	ASSERT_EQ(pmemfile_linkat(pfp, dir1, "", dir2, "XXX",
				  PMEMFILE_AT_EMPTY_PATH),
		  -1);
	EXPECT_EQ(errno, EPERM);

	PMEMfile *file1 = pmemfile_open(pfp, "/dir1/file1", PMEMFILE_O_RDONLY);
	ASSERT_NE(file1, nullptr) << strerror(errno);

	ASSERT_EQ(pmemfile_linkat(pfp, file1, "", dir2,
				  "file1-linked-at-empty-path",
				  PMEMFILE_AT_EMPTY_PATH),
		  0);

	ASSERT_TRUE(test_file_info(pfp, "/dir1/file1", 5, st_file1.st_ino));
	ASSERT_TRUE(test_file_info(pfp, "/dir2/file1-linked-at-empty-path", 5,
				   st_file1.st_ino));

	if (_pmemfile_fault_injection_enabled()) {
		pmemfile_gid_t groups[1] = {1002};
		ASSERT_EQ(pmemfile_setgroups(pfp, 1, groups), 0);
		_pmemfile_inject_fault_at(PF_MALLOC, 1, "copy_cred");
		errno = 0;
		ASSERT_EQ(pmemfile_linkat(pfp, PMEMFILE_AT_CWD, "dir1/file1",
					  PMEMFILE_AT_CWD, "file11", 0),
			  -1);
		EXPECT_EQ(errno, ENOMEM);
	}

	pmemfile_close(pfp, file1);
	pmemfile_close(pfp, dir1);
	pmemfile_close(pfp, dir2);

	ASSERT_EQ(pmemfile_unlink(pfp, "/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file1-link-to-deref-symlink"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file1-link-to-symlink"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir1/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file2"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file1-linked-at-empty-path"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir2/file1-sym"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir2"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir1"), 0);
}

/*
 * Test file handles created with O_PATH for all functions that accept
 * PMEMfile*. O_PATH allows to create file handles for files user does
 * not have read or write permissions. Such handles are supposed to be
 * used only as a path reference, but if we won't enforce that it may
 * become a security issue.
 */
TEST_F(dirs, O_PATH)
{
	char buf[4096];
	memset(buf, 0, sizeof(buf));

	ASSERT_EQ(pmemfile_mkdir(pfp, "/dir", PMEMFILE_S_IRWXU), 0);

	ASSERT_TRUE(test_pmemfile_create(pfp, "/dir/file", 0, 0));
	ASSERT_EQ(pmemfile_symlink(pfp, "/dir/file", "/dir/symlink"), 0);

	ASSERT_EQ(pmemfile_chmod(pfp, "/dir", PMEMFILE_S_IXUSR), 0);

	ASSERT_EQ(pmemfile_open(pfp, "/dir", 0), nullptr);
	EXPECT_EQ(errno, EACCES);

	PMEMfile *dir = pmemfile_open(
		pfp, "/dir", PMEMFILE_O_DIRECTORY /*ignored*/ |
			PMEMFILE_O_RDWR /*ignored*/ | PMEMFILE_O_PATH);
	ASSERT_NE(dir, nullptr);

	ASSERT_EQ(pmemfile_getdents(pfp, dir, (struct linux_dirent *)buf,
				    sizeof(buf)),
		  -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_getdents64(pfp, dir, (struct linux_dirent64 *)buf,
				      sizeof(buf)),
		  -1);
	EXPECT_EQ(errno, EBADF);

	PMEMfile *file =
		pmemfile_open(pfp, "/dir/file",
			      PMEMFILE_O_RDWR /*ignored*/ | PMEMFILE_O_PATH);
	ASSERT_NE(file, nullptr);

	ASSERT_EQ(pmemfile_read(pfp, file, buf, 10), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_pread(pfp, file, buf, 10, 0), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_write(pfp, file, buf, 10), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_pwrite(pfp, file, buf, 10, 0), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_lseek(pfp, file, 1, PMEMFILE_SEEK_SET), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_fchmodat(pfp, dir, "file",
				    PMEMFILE_S_IRUSR | PMEMFILE_S_IWUSR, 0),
		  0);

	PMEMfile *file2 = pmemfile_openat(pfp, dir, "file", PMEMFILE_O_RDWR);
	ASSERT_NE(file2, nullptr) << strerror(errno);

	memset(buf, 0xff, 10);
	ASSERT_EQ(pmemfile_write(pfp, file2, buf, 10), 10);
	ASSERT_EQ(pmemfile_lseek(pfp, file2, 0, PMEMFILE_SEEK_SET), 0);
	ASSERT_EQ(pmemfile_read(pfp, file2, &buf[100], 10), 10);
	EXPECT_EQ(memcmp(&buf[0], &buf[100], 10), 0);

	pmemfile_close(pfp, file2);

	pmemfile_stat_t st;

	memset(&st, 0xff, sizeof(st));
	ASSERT_EQ(pmemfile_fstat(pfp, file, &st), 0);
	EXPECT_EQ(st.st_size, 10);

	memset(&st, 0xff, sizeof(st));
	ASSERT_EQ(pmemfile_fstatat(pfp, dir, "file", &st, 0), 0);
	EXPECT_EQ(st.st_size, 10);

	memset(&st, 0xff, sizeof(st));
	ASSERT_EQ(pmemfile_fstatat(pfp, file, "", &st, PMEMFILE_AT_EMPTY_PATH),
		  0);
	EXPECT_EQ(st.st_size, 10);

	ASSERT_EQ(
		pmemfile_linkat(pfp, dir, "file", PMEMFILE_AT_CWD, "file1", 0),
		0);
	ASSERT_EQ(pmemfile_linkat(pfp, file, "", PMEMFILE_AT_CWD, "file2",
				  PMEMFILE_AT_EMPTY_PATH),
		  0);

	ASSERT_EQ(pmemfile_unlinkat(pfp, dir, "file", 0), -1);
	EXPECT_EQ(errno, EACCES);

	ASSERT_EQ(pmemfile_mkdirat(pfp, dir, "dir2", 0), -1);
	EXPECT_EQ(errno, EACCES);

	ASSERT_EQ(pmemfile_fchmod(pfp, file, PMEMFILE_S_IRWXU), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_fchmodat(pfp, dir, "file", PMEMFILE_S_IRWXU, 0), 0);

	ASSERT_EQ(pmemfile_fchown(pfp, file, 0, 0), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_fchownat(pfp, dir, "file", 0, 0, 0), 0);

	ASSERT_EQ(
		pmemfile_fchownat(pfp, file, "", 0, 0, PMEMFILE_AT_EMPTY_PATH),
		0);

	ASSERT_EQ(pmemfile_faccessat(pfp, dir, "file", PMEMFILE_W_OK, 0), 0);
	ASSERT_EQ(pmemfile_faccessat(pfp, NULL, "/dir/file", PMEMFILE_W_OK, 0),
		  0);
	ASSERT_EQ(pmemfile_faccessat(pfp, BADF, "/dir/file", PMEMFILE_W_OK, 0),
		  0);

	ASSERT_EQ(pmemfile_ftruncate(pfp, file, 0), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_fallocate(pfp, file, 0, 0, 1), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_symlinkat(pfp, "/file1", dir, "fileXXX"), -1);
	EXPECT_EQ(errno, EACCES);

	pmemfile_ssize_t r =
		pmemfile_readlinkat(pfp, dir, "symlink", buf, sizeof(buf));
	EXPECT_GT(r, 0);
	if (r > 0)
		EXPECT_EQ((size_t)r, strlen("/dir/file"));

	EXPECT_EQ(pmemfile_fcntl(pfp, dir, PMEMFILE_F_GETFL), PMEMFILE_O_PATH);
	EXPECT_EQ(pmemfile_fcntl(pfp, file, PMEMFILE_F_GETFL), PMEMFILE_O_PATH);

	EXPECT_EQ(pmemfile_fcntl(pfp, file, PMEMFILE_F_SETLK), -1);
	EXPECT_EQ(errno, EBADF);

	ASSERT_EQ(pmemfile_fchdir(pfp, dir), 0);
	ASSERT_EQ(pmemfile_access(pfp, "file", PMEMFILE_R_OK), 0);

	ASSERT_EQ(pmemfile_chdir(pfp, ".."), 0);

	pmemfile_close(pfp, dir);
	pmemfile_close(pfp, file);

	ASSERT_EQ(pmemfile_chmod(pfp, "/dir", PMEMFILE_S_IRWXU), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir/file"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/dir/symlink"), 0);
	ASSERT_EQ(pmemfile_rmdir(pfp, "/dir"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file1"), 0);
	ASSERT_EQ(pmemfile_unlink(pfp, "/file2"), 0);
}

/*
 * Test using root directories other than the default root.
 * This test assumes the number of root directories to be a compile time
 * constant, greater than one.
 */
TEST_F(dirs, root_dirs)
{
	if (root_count() == 0)
		return;

	PMEMfile *default_root;
	std::vector<PMEMfile *> roots;
	std::vector<pmemfile_stat_t> root_stats;
	pmemfile_stat_t default_root_stat;
	pmemfile_stat_t statbuf;

	ASSERT_EQ(pmemfile_pool_root_count(pfp), root_count());
	ASSERT_EQ(pmemfile_pool_root_count(nullptr), 0u);

	/* invalid pfp */
	ASSERT_EQ(pmemfile_open_root(nullptr, 0, 0), nullptr);
	ASSERT_EQ(errno, EINVAL);

	/* open the default root, which is expected to be the same as root #0 */
	default_root = pmemfile_open(pfp, "/", PMEMFILE_O_PATH);
	ASSERT_NE(default_root, nullptr) << strerror(errno);

	ASSERT_EQ(pmemfile_stat(pfp, "/", &statbuf), 0) << strerror(errno);
	ASSERT_EQ(pmemfile_fstat(pfp, default_root, &default_root_stat), 0)
		<< strerror(errno);

	ASSERT_EQ(PMEMFILE_S_ISDIR(statbuf.st_mode), 1);
	ASSERT_EQ(PMEMFILE_S_ISDIR(default_root_stat.st_mode), 1);
	ASSERT_EQ(statbuf.st_ino, default_root_stat.st_ino);

	/* open all valid roots */
	for (unsigned i = 0; i < root_count(); ++i) {
		PMEMfile *root = pmemfile_open_root(pfp, i, 0);
		pmemfile_stat_t rootstat;
		ASSERT_NE(root, nullptr) << strerror(errno);

		ASSERT_EQ(pmemfile_fstat(pfp, root, &rootstat), 0)
			<< strerror(errno);
		ASSERT_EQ(PMEMFILE_S_ISDIR(rootstat.st_mode), 1);

		/* No other root should have the same inode number */
		for (auto other_stat : root_stats)
			ASSERT_NE(other_stat.st_ino, rootstat.st_ino);

		/* create a regular file in each directory */
		PMEMfile *f = pmemfile_openat(
			pfp, root, "same_name",
			PMEMFILE_O_CREAT | PMEMFILE_O_EXCL, 0700);
		ASSERT_NE(f, nullptr) << strerror(errno);
		ASSERT_EQ(pmemfile_fstat(pfp, f, &statbuf), 0)
			<< strerror(errno);
		pmemfile_close(pfp, f);

		/* No other "same_name" should have the same inode number */
		for (auto other_root : roots) {
			pmemfile_stat_t other_stat;
			ASSERT_EQ(pmemfile_fstatat(pfp, other_root, "same_name",
						   &other_stat, 0),
				  0);
			ASSERT_NE(other_stat.st_ino, statbuf.st_ino);
		}

		ASSERT_NE(pmemfile_mkdirat(pfp, root, "./", 0700), 0);
		EXPECT_EQ(errno, EEXIST);

		roots.push_back(root);
		root_stats.push_back(rootstat);

		/* Make sure pmemfile doesn't pretend to support any flags */
		ASSERT_EQ(pmemfile_open_root(pfp, i, 2), nullptr);
		ASSERT_EQ(errno, EINVAL);

		ASSERT_EQ(pmemfile_open_root(pfp, i, -1), nullptr);
		ASSERT_EQ(errno, EINVAL);
	}

	/* root #0 is the default root */
	ASSERT_EQ(root_stats[0].st_ino, default_root_stat.st_ino);

	/* invalid root index */
	ASSERT_EQ(pmemfile_open_root(pfp, root_count(), 0), nullptr);
	ASSERT_EQ(errno, EINVAL);
	ASSERT_EQ(pmemfile_open_root(pfp, root_count() + 0xffff, 0), nullptr);
	ASSERT_EQ(errno, EINVAL);

	/* cleanup */
	pmemfile_close(pfp, default_root);
	for (auto root : roots) {
		ASSERT_EQ(pmemfile_unlinkat(pfp, root, "same_name", 0), 0);
		pmemfile_close(pfp, root);
	}
}

int
main(int argc, char *argv[])
{
	START();

	if (argc < 2) {
		fprintf(stderr, "usage: %s global_path [ops]", argv[0]);
		exit(1);
	}

	global_path = argv[1];

	if (argc >= 3)
		ops = (size_t)atoll(argv[2]);

	T_OUT("ops %zu\n", ops);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
