#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <smpi/smpi.h>

int main(int argc, char *argv[])
{
  int rank, pstates;
  int i;
  char buf[1024];
  char *s;
  size_t sz, x;

  if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
    printf("MPI initialization failed!\n");
    exit(EXIT_FAILURE);
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);   /* Get id of this process */

  pstates = smpi_get_host_nb_pstates();

  s = buf;
  sz = sizeof buf;
  x = snprintf(s, sz,
               "[%.6f] [rank %d] Pstates: %d; Powers: %.0f",
               MPI_Wtime(), rank, pstates, smpi_get_host_power_peak_at(0));
  if (x < sz) {
    s += x;
    sz -= x;
  } else
    sz = 0;
  for (i = 1; i < pstates; i++) {
    x = snprintf(s, sz, ", %.0f", smpi_get_host_power_peak_at(i));
    if (x < sz) {
      s += x;
      sz -= x;
    } else
      sz = 0;
  }
  printf("%s%s\n", buf, (sz ? "" : " [...]"));

  for (i = 0; i < pstates; i++) {
    smpi_set_host_power_peak_at(i);
    printf("[%.6f] [rank %d] Current pstate: %d; Current power: %.0f\n",
           MPI_Wtime(), rank, i, smpi_get_host_current_power_peak());

    SMPI_SAMPLE_FLOPS(1e9) {
      /* imagine here some code running for 1e9 flops... */
    }

    printf("[%.6f] [rank %d] Energy consumed: %g Joules.\n",
           MPI_Wtime(), rank, smpi_get_host_consumed_energy());
  }

  return MPI_Finalize();
}