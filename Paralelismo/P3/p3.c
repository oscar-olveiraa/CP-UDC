#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <math.h>

#define DEBUG 1

/* Translation of the DNA bases
   A -> 0
   C -> 1
   G -> 2
   T -> 3
   N -> 4*/

#define M  1000000 // Number of sequences
#define N  200  // Number of bases per sequence

unsigned int g_seed = 0;

int fast_rand(void) {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16) % 5;
}

// The distance between two bases
int base_distance(int base1, int base2){

    if((base1 == 4) || (base2 == 4)){
        return 3;
    }

    if(base1 == base2) {
        return 0;
    }

    if((base1 == 0) && (base2 == 3)) {
        return 1;
    }

    if((base2 == 0) && (base1 == 3)) {
        return 1;
    }

    if((base1 == 1) && (base2 == 2)) {
        return 1;
    }

    if((base2 == 2) && (base1 == 1)) {
        return 1;
    }

    return 2;
}

int main(int argc, char *argv[] ) {
    int numprocs , id;
    int i, j;
    int *data1, *data2;
    int *result;
    struct timeval  tv_comunication1, tv_comunication2, tv_computation1, tv_computation2;
    float comunication_time=0, computation_time=0;

    MPI_Init (&argc , &argv);
    MPI_Comm_size (MPI_COMM_WORLD , &numprocs);
    MPI_Comm_rank (MPI_COMM_WORLD , &id);

    int num_rows=ceil(1.0*M/numprocs);   //se multiplica por 1.0 para pasarlo a float pq ceil usa float
    int num_elements=num_rows*N;

    if(id==0){
        data1 = (int *) malloc(num_rows*numprocs*N*sizeof(int));
        data2 = (int *) malloc(num_rows*numprocs*N*sizeof(int));
        result = (int *) malloc(num_rows*numprocs*sizeof(int));
    }
    else{
        data1 = (int *) malloc(num_rows*N*sizeof(int));
        data2 = (int *) malloc(num_rows*N*sizeof(int));
        result = (int *) malloc(num_rows*sizeof(int));
    }

    if(id==0){
        /* Initialize Matrices */
        for(i=0;i<M;i++) {
            for(j=0;j<N;j++) {
                /* random with 20% gap proportion */
                data1[i*N+j] = fast_rand();
                data2[i*N+j] = fast_rand();
            }
        }
    }

    gettimeofday(&tv_comunication1, NULL);

    MPI_Scatter(data1, num_elements, MPI_INT, id?data1:MPI_IN_PLACE, num_elements, MPI_INT, 0, MPI_COMM_WORLD);   //si es el 0 recibe en MPI_IN_PLACE, sino recibe en data1
    MPI_Scatter(data2, num_elements, MPI_INT, id?data2:MPI_IN_PLACE, num_elements, MPI_INT, 0, MPI_COMM_WORLD);

    gettimeofday(&tv_comunication2, NULL);
    comunication_time += (tv_comunication2.tv_usec - tv_comunication1.tv_usec)+ 1000000 * (tv_comunication2.tv_sec - tv_comunication1.tv_sec);

    gettimeofday(&tv_computation1, NULL);

    int num_rows2;
    if(id<(numprocs-1)) num_rows2=num_rows;    //si no es el ultimo proceso se calcula todo
    else num_rows2=M-ceil(1.0*M/numprocs)*(numprocs-1);   //si es el ultimo se elimina las filas sobrantes

    for(i=0;i<num_rows2;i++) {
        result[i]=0;
        for(j=0;j<N;j++) {
            result[i] += base_distance(data1[i*N+j], data2[i*N+j]);
        }
    }


    gettimeofday(&tv_computation2, NULL);
    computation_time += (tv_computation2.tv_usec - tv_computation1.tv_usec)+ 1000000 * (tv_computation2.tv_sec - tv_computation1.tv_sec);

    gettimeofday(&tv_comunication1, NULL);

    MPI_Gather(id?result:MPI_IN_PLACE, num_rows, MPI_INT, result, num_rows, MPI_INT, 0, MPI_COMM_WORLD);

    gettimeofday(&tv_comunication2, NULL);
    comunication_time += (tv_comunication2.tv_usec - tv_comunication1.tv_usec)+ 1000000 * (tv_comunication2.tv_sec - tv_comunication1.tv_sec);


    /* Display result */
    if (DEBUG == 1) {
        if(id==0){
            int checksum = 0;
            for(i=0;i<M;i++) {
                checksum += result[i];
            }
            printf("Checksum: %d\n ", checksum);
        }
    } else if (DEBUG == 2) {
        if(id==0){
            for(i=0;i<M;i++) {
                printf(" %d \t ",result[i]);
            }
        }
    } else {
        printf ("Comunication time (seconds) process %d= %lf\n", id, (double) comunication_time/1E6);
        printf ("Computation time (seconds) process %d= %lf\n", id, (double) computation_time/1E6);
    }

    free(data1); free(data2); free(result);

    MPI_Finalize();

    return 0;
}



