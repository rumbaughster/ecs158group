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


int *numcount(int *x, int n, int m) {
  // For this version, we will use a map as a global hash table that all threads access.
  // For now, we're using strings as keys for the hashtable. We cannot use an array of ints, which would be ideal,
  // because you must use a constant value as a key - c++ doesn't want you to modify the key while it's in the hash table.
  int* bitsize = (int*)malloc(sizeof(int));
  *bitsize = ceil(log2(n-m+1));
  int* hashtablelength =(int*)malloc(sizeof(int));
  *hashtablelength = pow(2,*bitsize);
  node** hashtable = (node**) malloc(sizeof(node**)*(*hashtablelength));
  int *subsequence_arr;
  // Array of locks for individual locking
  omp_lock_t* lock = (omp_lock_t*)malloc(sizeof(omp_lock_t*)*(*hashtablelength));
  int* outputarray = (int*) malloc(sizeof(int*) * (1 + (n-m+1)*(m+1)) );
  outputarray[0] = 0;
  int* currsubseqno = (int*) malloc(sizeof(int*));
  *currsubseqno = 0;
 
  
  int* numThreads = (int*)malloc(sizeof(int*));
  
  omp_lock_t* outputindexlock = (omp_lock_t*)malloc(sizeof(omp_lock_t*));
  omp_init_lock(outputindexlock);
  for(int i = 0; i < *hashtablelength; i++)
  {
    hashtable[i] = NULL;
    //initializes the array of locks
    omp_init_lock(&(lock[i])); 
  }

  // Start the threads
  #pragma omp parallel //for shared(lock)
  {    
    int offset = omp_get_thread_num();
    *numThreads = omp_get_num_threads();
    int outputindex = 0;
    int amt;
    
    //subsequence_arr[offset] = 0;
    #pragma omp for
    for(int i = 0 ; i < n-m+1 ; i++) {   
      // Don't write without starting a critical section
      uint32_t hash32 = hash(&x[i],m);
      hash32 = (((hash32>>*bitsize) ^ hash32) & TINY_MASK(*bitsize));
 
      omp_set_lock(&(lock[hash32]));
      {
         //no entry yet for hash
         if(hashtable[hash32] == NULL)
         {
          node* newnode = (node*)malloc(sizeof(node*));
          newnode->array = (int**)malloc(sizeof(int**));
          //aquire lock
          omp_set_lock(outputindexlock);
          {
            outputindex = *currsubseqno;
            outputarray[0]++;
            (*currsubseqno)++;
          }
          omp_unset_lock(outputindexlock);
          //load into the outputarray
          for(int j =0; j < m; j++)
          {
            outputarray[1+ outputindex * (m+1) + j] = x[i+j];
          }
          outputarray[1+ outputindex * (m+1) + m] = 1;
          newnode->array[0] = &outputarray[1+ outputindex * (m+1)];
          newnode->amount = 1;
          hashtable[hash32] = newnode;
         }
        //possible collision
         else
         {
          int collision = 1;
	  //Save the amount in a variable to save on multiple redundant reads from hash table
          amt = hashtable[hash32]->amount;
          for(int j = 0; j < amt ; j++)
          {
            if(compareArray(hashtable[hash32]->array[j],&x[i],m) == 1)
            {
                (hashtable[hash32]->array[j][m])++;                  
                collision = 0;
                break;
            }
          }
          //collision
          if(collision == 1)
          {
            //reallocate the size
            hashtable[hash32]->array = (int**)realloc( hashtable[hash32]->array , ((hashtable[hash32]->amount)+1)*sizeof(int**) );
            //aquire lock
            omp_set_lock(outputindexlock);
            {
              outputindex = *currsubseqno;
              outputarray[0]++;
              (*currsubseqno)++;
            }
            omp_unset_lock(outputindexlock);
            
            //load into the outputarray
            for(int j =0; j < m; j++)
            {
              outputarray[1+ outputindex * (m+1) + j] = x[i+j];
            }
            outputarray[1+ outputindex * (m+1) + m] = 1;
            hashtable[hash32]->array[hashtable[hash32]->amount] = &outputarray[1+ outputindex * (m+1)];
            (hashtable[hash32]->amount)++;      
          }     
         }
      } // end critical section
      omp_unset_lock(&(lock[hash32]));
    } // reached end of array
    //wait for all the threads to finish
  
    #pragma omp for
    for(int i = 0; i < *hashtablelength; i++)
    {
      /*
      if(hashtable[i] !=NULL)
      {
        free(hashtable[i]->array);
        free(hashtable[i]);
      }
      */
      omp_destroy_lock(&(lock[i]));
    }
  } // end of parallel processing. Implied break

  
  omp_destroy_lock(outputindexlock);
  /*
  NOTE:
  Spoke with the TA and he told us we could avoid freeing memory to achieve a faster time.
  But we will leave the free commands in as comments to demonstrate we understand how we would free it.
  */
  
  /*
  free(bitsize);
  free(hashtablelength);
  free(currsubseqno);
  free(numThreads);
  
  
  free(hashtable);
  free(lock);
*/
  return(outputarray);
} 
