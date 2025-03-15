#pragma once
#include <cuda_runtime.h>
#include <optix.h> 
#include <optix_types.h>
class RealTimeDenoiser
{

private:
OptixDeviceContext context;
OptixDenoiser denoiser;

public:
    RealTimeDenoiser();
    ~RealTimeDenoiser();

    void denoise(const float* color, const float* albedo, const float* normal, const float* output, int width, int height);
};