#define _GNU_SOURCE

#include <sys/io.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#define GROUP_PATH "/sys/fs/resctrl/mon_groups/m"
struct mon_group {
	char *path;
};

static struct mon_group *mon_group_new(void)
{
	static unsigned int next_id = 0;
	struct mon_group *mg = malloc(sizeof(*mg));
	char *tmp_path;
	int res;

	asprintf(&mg->path, GROUP_PATH "%u", next_id);

	rmdir(mg->path);

	res = mkdir(mg->path, 0777);
	if (res) {
		perror("mkdir");
		abort();
	}

	next_id++;

	return mg;
}

static void mon_group_destroy(struct mon_group *mg)
{
	rmdir(mg->path);
	free(mg->path);
	free(mg);
}

static void count_chunk(void)
{
	outl(1, 0xd0c);
}

static void mon_group_chunk(struct mon_group *mg)
{
	char *buf;
	char *tmp_path;
	int fd;

	asprintf(&tmp_path, "%s/tasks", mg->path);
	fd = open(tmp_path, O_RDWR);
	if (fd < 0) {
		perror("open");
		abort();
	}
	free(tmp_path);

	asprintf(&buf, "%d\n", getpid());
	write(fd, buf, strlen(buf));
	free(buf);
	close(fd);

	count_chunk();
}

static int64_t mon_group_total_domain(struct mon_group *mg, int domain_id)
{
	unsigned long total;
	int r;
	char buf[100];
	char *tmp_path;
	int fd;

	asprintf(&tmp_path, "%s/mon_data/mon_L3_%02d/mbm_total_bytes", mg->path,
		domain_id);
	fd = open(tmp_path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return -1;
		perror("open");
		abort();
	}
	free(tmp_path);

	ssize_t b = read(fd, buf, sizeof(buf));
	if ((r = sscanf(buf, "%lu", &total)) != 1) {
		buf[b] = 0;
		printf("sscanf failed mbm_total_bytes: '%s'\n", buf);
		return 0;
	}

	return total;
}

static uint64_t mon_group_total(struct mon_group *mg)
{
	uint64_t total = 0;
	int domain_id = 0;
	int64_t result;

	while (true) {
		result = mon_group_total_domain(mg, domain_id);
		if (result == -1)
			break;
		assert((total + (uint64_t)result) >= total);
		total += (uint64_t)result;
		domain_id++;
	}

	return total;
}

int main(void)
{
	struct mon_group *m1, *m2;
	int ret;
	int i;

	ret = ioperm(0xd0c, 4, 1);
	if (ret != 0) {
		perror("ioperm");
		return 1;
	}

	m1 = mon_group_new();
	m2 = mon_group_new();

	printf("m1->%lu m2->%lu\n", mon_group_total(m1), mon_group_total(m2));

	mon_group_chunk(m1);
	mon_group_chunk(m1);
	mon_group_chunk(m2);

	printf("m1->%lu m2->%lu\n", mon_group_total(m1), mon_group_total(m2));

	mon_group_destroy(m1);
	mon_group_destroy(m2);

	return 0;
}
