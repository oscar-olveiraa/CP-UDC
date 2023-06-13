#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

void inicializaCadena(char *cadena, int n){
    int i;
    for(i=0; i<n/2; i++){
        cadena[i] = 'A';
    }
    for(i=n/2; i<3*n/4; i++){
        cadena[i] = 'C';
    }
    for(i=3*n/4; i<9*n/10; i++){
        cadena[i] = 'G';
    }
    for(i=9*n/10; i<n; i++){
        cadena[i] = 'T';
    }
}

int main(int argc, char *argv[])
{
    int numprocs , id, n, count=0, count_parcial=0;
    char *cadena;
    char L;

    MPI_Init (&argc , &argv);
    MPI_Comm_size (MPI_COMM_WORLD , &numprocs);
    MPI_Comm_rank (MPI_COMM_WORLD , &id);

    if(id==0){

        if(argc != 3){
            printf("Numero incorrecto de parametros\nLa sintaxis debe ser: program n L\n  program es el nombre del ejecutable\n  n es el tamaño de la cadena a generar\n  L es la letra de la que se quiere contar
            apariciones (A, C, G o T)"\n);
            exit(1);
        }

        n = atoi(argv[1]);//tamaño cadena
        L = *argv[2];//elementos da cadena

        for(int i=1; i<numprocs; i++){
            MPI_Send(&n, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

        for(int i=1; i<numprocs; i++){
            MPI_Send(&L, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        }

    }else{
        MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&L, 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    cadena = (char *) malloc(n*sizeof(char));
    inicializaCadena(cadena, n);

    for(int i=id; i<n; i+=numprocs){
        if(cadena[i] == L){
            count++;
        }
    }

    if(id==0){
        for(int i=1; i<numprocs; i++){
            MPI_Recv(&count_parcial, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            count+=count_parcial;//necesitase unha variable global para que non machaque no count
        }
        printf("El numero de apariciones de la letra %c es %d\n", L, count);
    }
    else{
        MPI_Send(&count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    free(cadena);
    exit(0);
}