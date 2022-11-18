#include <sys/io.h>
#include <stdio.h>

int main(void)
{
	int ret;
	int i;

	ret = ioperm(0xd0c, 4, 1);
	if (ret != 0) {
		perror("ioperm");
		return 1;
	}

	outl(1, 0xd0c);

	return 0;
}
