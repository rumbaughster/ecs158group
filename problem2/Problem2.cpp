// Ryan King
// I ran this in the CSIF with the following compile command:
// g++ -fopenmp -o P2.out Problem2.cpp

#define TINY_MASK(x) (((u_int32_t)1<<(x))-1)

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
// We are using an unordered map as our hash table for this version
#include <tr1/unordered_map>
#include <string>
#include <sstream>
#include <iostream>
#include <iterator>
#include <ctime>
#include <sys/time.h>
#include <math.h>
#include <stdint.h>
#include <cstring>
#include <iomanip>

using namespace std;
int* numcount(int *x, int n, int m);
double get_wall_time();
void printOutputArray(int* array, int m);


int main( int argc, const char* argv[] ) {
  srand (time(NULL));  
  double begin, end;
  printf( "\nStarting...\n");

  // length of array
  int n = atoi(argv[1]);
  // length of patterns  
  int m = atoi(argv[2]);

  // building the array to search through
  int* x = new int[n];
  for(int i = 0 ; i < n ; i++)
  {
    x[i] = rand() % 101;
    //x[i] = 10;
    //cout << x[i] << " ";
  }
  cout << "\n";
  
  begin = get_wall_time();
  int* output = numcount(x,n,m);
  end = get_wall_time();
  //printOutputArray(output, m);
 
  cout << "Execution time: " << end - begin << "\n";
  return(0);
}




//hashing function
//FNV hashing algorithm
// http://www.isthe.com/chongo/tech/comp/fnv/
uint32_t hash(int *data, int length)
{
    uint32_t hash = 2166136261;
    unsigned int FNV_prime = 16777619;
    for (int i = 0; i < length; i++)
    {
      hash = hash ^ data[i];
      hash = hash * FNV_prime;
    }
    return hash;
}
 
//
struct node{
  int** array;
  int* count;
  int amount;
};

int compareArray(int* array1, int* array2, int m)
{
  for (int i = 0; i < m; i++)
  {
    if(array2[i] != array1[i])return 0;
  }
  return 1;
}

void printArraySequence(int* array, int m)
{
  for(int k = 0; k < m; k++)
  {
    printf("%d, ",array[k]);
  }
  
  
}

string keyFromArray(int* array,int length);

int *numcount(int *x, int n, int m) {
  // For this version, we will use a map as a global hash table that all threads access.
  // For now, we're using strings as keys for the hashtable. We cannot use an array of ints, which would be ideal,
  // because you must use a constant value as a key - c++ doesn't want you to modify the key while it's in the hash table.
  

  int bitsize = ceil(log2(n-m+1));
  int hashtablelength = pow(2,bitsize);
  node** hashtable = (node**) malloc(sizeof(node**)*(hashtablelength));
  // Number of unique subsequences
  int subsequences = 0;
  // Array of locks for individual locking
  omp_lock_t* lock = (omp_lock_t*)malloc(sizeof(omp_lock_t*)*(hashtablelength));
  // Array of hashtable indices, size of worst case number of unique subseq
  uint32_t* hash_index = (uint32_t*) malloc(sizeof(uint32_t*)*(n-m+1));
  // Number of indices in hash_index
  int hash_index_count = 0;

  omp_lock_t* subseqcountlock = (omp_lock_t*)malloc(sizeof(omp_lock_t*));
  omp_init_lock(subseqcountlock);

  for(int i = 0; i < hashtablelength; i++)
  {
    hashtable[i] = NULL;
    //initializes the array of locks
    omp_init_lock(&(lock[i])); 
  }
  //int* sequence = new int[1000];
  // Start the threads
  #pragma omp parallel //for shared(lock)
  {
    
    double setup_time = get_wall_time(), begin, thread_time, wait_time = 0, hash_time = 0, critical_time = 0;
    thread_time = get_wall_time();
    int offset = omp_get_thread_num();
    int numThreads = omp_get_num_threads();
    #pragma omp single
    {
      printf("Num threads = %d ", numThreads);
      printf("length = %d ", hashtablelength);
      printf("\n");
      printf("%s \t %s \t %s \t %s \t %s \t %s\n","No","Setup","Wait","Hash","Crit", "Thread");
    }
    setup_time = get_wall_time() - setup_time;
    #pragma omp for
    for(int i = 0 ; i < n-m+1 ; i++) {   
      // Don't write without starting a critical section
      begin = get_wall_time();
      uint32_t hash32 = hash(&x[i],m);
      hash32 = (((hash32>>bitsize) ^ hash32) & TINY_MASK(bitsize));
      hash_time += get_wall_time() - begin;
      begin = get_wall_time();
 
//      #pragma omp critical
      omp_set_lock(&(lock[hash32]));
      {
         wait_time += get_wall_time()-begin;
         begin = get_wall_time();
         //no entry yet for hash
         if(hashtable[hash32] == NULL)
         {
          //add to hash_index;
          #pragma omp critical
          {
            hash_index[hash_index_count] = hash32;
            hash_index_count++;
          }
          node* newnode = (node*)malloc(sizeof(node*));
          newnode->array = (int**)malloc(sizeof(int**));
          newnode->array[0] = &x[i];
          newnode->count = (int*)malloc(sizeof(int*));
          newnode->count[0] = 1;
          newnode->amount = 1;
          hashtable[hash32] = newnode;
          omp_set_lock(subseqcountlock);
          {
                        
            subsequences++;
          }
          omp_unset_lock(subseqcountlock);
         }
         //possible collision
         //only add hash32 to hash_index if it hasn't been used before
         //  will later loop through array to get all substr
         else
         {
          int collision = 1;
          for(int j = 0; j < hashtable[hash32]->amount ; j++)
          {
            if(compareArray(hashtable[hash32]->array[j],&x[i],m) == 1)
            {
                (hashtable[hash32]->count[j])++;
                collision = 0;
                break;
            }
          }
          //collision
          if(collision == 1)
          {
            //reallocate the size
            hashtable[hash32]->array = (int**)realloc( hashtable[hash32]->array , ((hashtable[hash32]->amount)+1)*sizeof(int**) );
            hashtable[hash32]->count = (int*)realloc( hashtable[hash32]->count, ((hashtable[hash32]->amount)+1)*sizeof(int*) );
             
            hashtable[hash32]->array[hashtable[hash32]->amount] = &x[i];
            hashtable[hash32]->count[hashtable[hash32]->amount] = 1;
            (hashtable[hash32]->amount)++;
            omp_set_lock(subseqcountlock);
            {
              subsequences++;
            }
            omp_unset_lock(subseqcountlock);
          }     
         }
       critical_time += get_wall_time() - begin;
      } // end critical section
      omp_unset_lock(&(lock[hash32]));

    } // reached end of array
    //wait for all the threads to finish
    thread_time = get_wall_time() - thread_time;
    #pragma omp barrier
    #pragma omp critical
    { 
      printf("%d \t %.2f \t %.2f \t %.2f \t %.2f \t %.2f\n", offset, 
      setup_time, wait_time,hash_time,
      critical_time, thread_time);
    }
  } // end of parallel processing. Implied break


  //now we will place the results into the output array
  double begin = get_wall_time();
  int num_of_subseq = subsequences*(m+1);
  int* outputarray = (int*) malloc(sizeof(int*) * (1+num_of_subseq));
  int currsubseqno = 0;

  outputarray[0] = subsequences;
  
  //copying out the elements of the hash table to outputarray
  //#pragma omp parallel for   
  for(int i = 0; i < hash_index_count; i++) 
  {  
    for(int j = 0; j < hashtable[hash_index[i]]->amount; j++)
    {
      int outputindex = 0;
      //#pragma omp critical
      {
        outputindex = currsubseqno;
        currsubseqno++;
      } 
      for(int k = 0; k < m; k++)
      {
        outputarray[1 + outputindex*(m+1)+k] = hashtable[hash_index[i]]->array[j][k];
      }
      outputarray[1 + outputindex*(m+1)+m]=hashtable[hash_index[i]]->count[j];
    }
    free(hashtable[hash_index[i]]->count);
    free(hashtable[hash_index[i]]->array);
    free(hashtable[hash_index[i]]);
  }
  printf("HELLO!\n");
  // go through and destroy the locks
  #pragma omp parallel for
  for(int i = 0; i <  hashtablelength; i++)
  {
    omp_destroy_lock(&(lock[i]));
  }


  free(hashtable);
  free(lock);
  printf("\n");
  return(outputarray);
} 


void printOutputArray(int* array, int m)
{
  int length = array[0];
  for(int i = 0; i < length; i++)
  {
    printf("Subsequence: ");
    for(int j = 0; j<m;j++)
    {
      printf("%d, ", array[1 + i*(m+1)+j]);
    }
    printf(" Count: %d\n", array[1 + i*(m+1)+m]); 
  }
}

double get_wall_time(){
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}
