#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define OUTPUT "output_c.txt"
#define SIZE 500

void populate(float m1[][SIZE], float m2[][SIZE])
{
	int i, j;
	for (i=0; i<SIZE; i++) {
		for (j=0; j<SIZE; j++) {
			m1[i][j] = 1.0f;
			m2[i][j] = 2.0f;
		}
	}
}

void add_m(float m1[][SIZE], float m2[][SIZE], float res[][SIZE])
{
	int i, j;
	for (i=0; i<SIZE; i++)
		for (j=0; j<SIZE; j++)
			res[i][j] = m1[i][j] + m2[i][j];
}

void print_results(float res[][SIZE])
{
	int i, j;
	FILE *out;
	out = fopen(OUTPUT, "w");
	if (out == NULL) {
		printf("Creating output file failed\n");
		exit(1);
	}

	for (i=0; i<SIZE; i++)
		for (j=0; j<SIZE; j++)
			fprintf(out, "%f\n", res[i][j]);
}

int main(void)
{
	float m1[SIZE][SIZE], m2[SIZE][SIZE], res[SIZE][SIZE];
	clock_t s, e, t;

	populate(m1, m2);

	s = clock();

	add_m(m1, m2, res);

	e = clock();
	t = (e -s);

	print_results(res);
	printf("clock ticks: %ld\n", t);
	printf("\n");
	return 0;
}





