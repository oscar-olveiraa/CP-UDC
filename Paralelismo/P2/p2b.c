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

int MPI_FlattreeColectiva(void * buff, void *recvbuff, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){
    int numprocs, id, count_total, count_parcial;
    int estado;
    MPI_Comm_size(comm,&numprocs);
    MPI_Comm_rank(comm,&id);
    if(id==root){
        count_total= *(int*) buff;
        for(int i=0; i<numprocs; i++){
            if(i!=root){
                estado=MPI_Recv(&count_parcial, count, datatype, i, 0, comm, MPI_STATUS_IGNORE);
                if(estado!=MPI_SUCCESS) return estado;
                count_total+=count_parcial;
            }
        }
        *(int*) recvbuff=count_total;
    }
    else{
        estado=MPI_Send(buff, count, datatype, root, 0, comm);
        if(estado!=MPI_SUCCESS) return estado;
    }
    return MPI_SUCCESS;
}

int MPI_BinomialBcast(void *buff, int count, MPI_Datatype datatype, int root, MPI_Comm comm){
    int numprocs, id;
    int estado;
    MPI_Comm_size(comm,&numprocs);
    MPI_Comm_rank(comm,&id);

    if(root!=0) return MPI_ERR_ROOT;
    if(id!=0){
        estado=MPI_Recv(buff, count, datatype, MPI_ANY_SOURCE, 0, comm, MPI_STATUS_IGNORE);  //MPI_ANY_SOURCE recive de cualquier procerso
        if(estado!=MPI_SUCCESS) return estado;
    }
    for(int k=1; ;k<<=1){ //k representa pow(2, i), k<<=1 añade un bit a la derecha por lo que equivale a multiplicar por 2
        if(id<k){
            int destino=id+k;
            if(destino>=numprocs) break;
            estado=MPI_Send(buff, count, datatype, destino, 0, comm);
            if(estado!=MPI_SUCCESS) return estado;
        }
    }
    return MPI_SUCCESS;
}

int main(int argc, char *argv[])
{
    int numprocs , id, n, count=0, count_total=0;
    char *cadena;
    char L;

    MPI_Init (&argc , &argv);
    MPI_Comm_size (MPI_COMM_WORLD , &numprocs);
    MPI_Comm_rank (MPI_COMM_WORLD , &id);

    if(id==0){

        if(argc != 3){
            printf("Numero incorrecto de parametros\nLa sintaxis debe ser: program n L\n  program es el nombre del ejecutable\n  n es el tamaño de la cadena a generar\n  L es la letra de la que se quiere contar apariciones (A, C, G o T)\n");
            exit(1);
        }

        n = atoi(argv[1]);
        L = *argv[2];
    }

    MPI_BinomialBcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_BinomialBcast(&L, 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    cadena = (char *) malloc(n*sizeof(char));
    inicializaCadena(cadena, n);

    for(int i=id; i<n; i+=numprocs){
        if(cadena[i] == L){
            count++;
        }
    }

    MPI_FlattreeColectiva(&count, &count_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);


    if(id==0){
        printf("El numero de apariciones de la letra %c es %d\n", L, count_total);
    }

    MPI_Finalize();

    free(cadena);
    exit(0);
}
