#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>

__global__ void vecAdd(const float *a, const float *b, float *c, unsigned n) {
    unsigned int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n){
      c[i] = a[i] + b[i];
    } 
}

int main(int argc, char **argv) {
    unsigned N = 3000;
    size_t bytes = N * sizeof(float);

    float *h_a = (float*)malloc(bytes);
    float *h_b = (float*)malloc(bytes);
    float *h_c = (float*)malloc(bytes);
    
    if (!h_a || !h_b || !h_c) { 
      exit(1);
    }

    for (int i = 0; i < N; ++i) { 
      h_a[i] = i * 1.0; 
      h_b[i] = i * 0.5f; 
    }

    float *d_a = nullptr;
    float *d_b = nullptr;
    float *d_c = nullptr;

    if (cudaMalloc((void**)&d_a, bytes) != cudaSuccess) { 
      exit(1);
    }
    if (cudaMalloc((void**)&d_b, bytes) != cudaSuccess) { 
      exit(1);
    }
    if (cudaMalloc((void**)&d_c, bytes) != cudaSuccess) { 
      exit(1);
    }

    cudaMemcpy(d_a, h_a, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, h_b, bytes, cudaMemcpyHostToDevice);

    unsigned block = 256;
    unsigned grid = (N + block - 1) / block;

    vecAdd<<<grid, block>>>(d_a, d_b, d_c, N);
    // culaucnhkernel(grid, block, parameters)

    cudaDeviceSynchronize();

    cudaMemcpy(h_c, d_c, bytes, cudaMemcpyDeviceToHost);

    for (int i = 0; i < N; ++i) { 
      assert(h_c[i] == h_a[i] + h_b[i]);
    }
    
    std::cout << "correct, exiting\n";

    cudaFree(d_a); 
    cudaFree(d_b); 
    cudaFree(d_c);
    free(h_a); 
    free(h_b); 
    free(h_c);
    
    return 0;
}
