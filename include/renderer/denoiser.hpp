#pragma once
#include <cuda_runtime.h>
#include <optix.h> 
#include <optix_types.h>
class RealTimeDenoiser
{

private:
OptixDeviceContext context;
OptixDenoiser denoiser;
OptixDenoiserSizes denoiser_sizes;
void* denoiser_state;
void* denoiser_scratch;

public:
    RealTimeDenoiser(unsigned int width, unsigned int height);
    ~RealTimeDenoiser();

    // input image buffers should be in CUDA device memory
    void denoise(const float* color, const float* albedo, const float* normal, const float* output, int width, int height);
};