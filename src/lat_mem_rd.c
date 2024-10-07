/*
 * lat_mem_rd.c - measure memory load latency
 *
 * usage: lat_mem_rd [-P <parallelism>] [-W <warmup>] [-N <repetitions>] [-t] size-in-MB [stride ...]
 *
 * Copyright (c) 1994 Larry McVoy.
 * Copyright (c) 2003, 2004 Carl Staelin.
 *
 * Distributed under the FSF GPL with additional restriction that results
 * may published only if:
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
 */
char *id = "$Id: s.lat_mem_rd.c 1.13 98/06/30 16:13:49-07:00 lm@lm.bitmover.com $\n";

#include "bench.h"
#define STRIDE (512 / sizeof(char *)) // Defaults to 64
#define LOWER 16 * 1024	// Minimum buffer size is 16 KB
void loads(size_t len, size_t range, size_t stride,
		   int parallel, int warmup, int repetitions);
void initialize(iter_t iterations, void *cookie);

benchmp_f fpInit = stride_initialize;

int main(int ac, char **av)
{
	int i;
	int c;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	size_t len;
	size_t range;
	size_t stride;
	char *usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>] [-t] len [stride...]\n";

	while ((c = getopt(ac, av, "tP:W:N:")) != EOF)
	{
		switch (c)
		{
		case 't':
			fpInit = thrash_initialize;
			break;
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0)
				lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind == ac)
	{
		lmbench_usage(ac, av, usage);
	}

	// Length multiplied to be MB
	len = atoi(av[optind]);
	len *= 1024 * 1024;

	if (optind == ac - 1)
	{
		// DEFAULT: stride is 64. There is only the inner loop,
		// which is the array size. The array size goes from 512
		// bytes to the max length specified in the args.
		fprintf(stderr, "Stride: %d\n", STRIDE);

		for (range = LOWER; range <= len; range *= 2)
		{
			loads(len, range, STRIDE, parallel,
				  warmup, repetitions);
		}

		// If we're just testing exactly size-in-MB, run this:
		// loads(len, len, STRIDE, parallel, warmup, repetitions);
	}
	else
	{
		// The benchmark runs as two nested loops. The outer loop
		// is the stride size (for each stride). This doesn't run
		// unless we specify at least one custom stride.
		for (i = optind + 1; i < ac; ++i)
		{
			stride = bytes(av[i]);
			fprintf(stderr, "Stride: %d\n", stride);

			// The inner loop is the array size.
			for (range = LOWER; range <= len; range *= 2)
			{
				loads(len, range, stride, parallel,
					  warmup, repetitions);
			}
			fprintf(stderr, "\n");
		}
	}
	return (0);
}

// Loop unrolling, lol
#define ONE p = (char **)*p;
#define FIVE ONE ONE ONE ONE ONE
#define TEN FIVE FIVE
#define FIFTY TEN TEN TEN TEN TEN
#define HUNDRED FIFTY FIFTY

// This is the function where the interesting stuff happens...
// There is a ring (circular linked list) of pointers that
// point backward one stride.
void benchmark_loads(iter_t iterations, void *cookie)
{
	struct mem_state *state = (struct mem_state *)cookie;
	register char **p = (char **)state->p[0];
	register size_t i;
	register size_t count = state->len / (state->line * 100) + 1;

	while (iterations-- > 0)
	{
		for (i = 0; i < count; ++i)
		{
			HUNDRED;
		}
	}

	// Seems like this prevents the ptr-chase from being
	// optimized out by the compiler
	use_pointer((void *)p);
	state->p[0] = (char *)p;
}

void loads(size_t len, size_t range, size_t stride,
		   int parallel, int warmup, int repetitions)
{
	double result;
	size_t count;
	struct mem_state state;

	if (range < stride)
		return;

	state.width = 1;
	state.len = range;
	state.maxlen = len;
	state.line = stride;
	state.pagesize = getpagesize();
	count = 100 * (state.len / (state.line * 100) + 1);

	/*
	 * Now walk them and time it.
	 * nosajmik: need to figure out what this crap does.
	 */
	benchmp(fpInit, benchmark_loads, mem_cleanup,
			100000, parallel, warmup, repetitions, &state);

	/* We want to get to nanoseconds / load. */
	save_minimum();
	result = (1000. * (double)gettime()) / (double)(count * get_n());
	fprintf(stderr, "Buffer size: %d KB, Avg. Load Latency: %.3f ns\n", range / 1024, result);
}
