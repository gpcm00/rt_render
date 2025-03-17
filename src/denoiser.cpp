#include <renderer/denoiser.hpp>
#include <optix_stubs.h>
#include <optix_function_table_definition.h>
#include <optix_function_table.h>
#include <iostream>

RealTimeDenoiser::RealTimeDenoiser(unsigned int width, unsigned int height) {
    // Initialize OptiX context and denoiser
    std::cout << "Initializing OptiX denoiser..." << std::endl;

    // Initialize the OptiX device context
    cudaSetDevice(0);


    OptixDeviceContextOptions options = {};
    options.logCallbackFunction = nullptr;
    options.logCallbackLevel = 4;  // Default log level

    optixInit();

    if (optixDeviceContextCreate(0, &options, &context) != OPTIX_SUCCESS)
    {
        std::cerr << "Failed to create OptiX context." << std::endl;
        throw std::runtime_error("Failed to create OptiX context.");
    }
    std::cout << "OptiX context created successfully." << std::endl;

    // https://raytracing-docs.nvidia.com/optix9/api/group__optix__host__api__denoiser.html
    // Initialize the OptiX denoiser
   
    OptixDenoiserModelKind model = OPTIX_DENOISER_MODEL_KIND_AOV;
    OptixDenoiserOptions dopt = {};
    dopt.guideAlbedo = 0;
    dopt.guideNormal = 0;
    dopt.denoiseAlpha = OptixDenoiserAlphaMode::OPTIX_DENOISER_ALPHA_MODE_COPY;

    if (optixDenoiserCreate(context, model, &dopt, &denoiser) != OPTIX_SUCCESS)
    {
        std::cerr << "Failed to create OptiX denoiser." << std::endl;
        throw std::runtime_error("Failed to create OptiX denoiser.");
    }

    std::cout << "OptiX denoiser created successfully." << std::endl;

    std::cout << "Setting up OptiX denoiser..." << std::endl;
    // Set up the denoiser
    OptixDenoiserSizes denoiser_sizes;
    optixDenoiserComputeMemoryResources(denoiser, width, height, &denoiser_sizes);

    cudaMalloc(&denoiser_state, denoiser_sizes.stateSizeInBytes);
    cudaMalloc(&denoiser_scratch, denoiser_sizes.withoutOverlapScratchSizeInBytes);

    optixDenoiserSetup(denoiser, nullptr, 
        width, 
        height, 
        reinterpret_cast<CUdeviceptr>(denoiser_state), 
        denoiser_sizes.stateSizeInBytes, 
        reinterpret_cast<CUdeviceptr>(denoiser_scratch), 
        denoiser_sizes.withoutOverlapScratchSizeInBytes
    );

}

RealTimeDenoiser::~RealTimeDenoiser()
{
    // Clean up OptiX resources
    std::cout << "Cleaning up OptiX resources..." << std::endl;
    cudaFree(denoiser_scratch);
    cudaFree(denoiser_state);
    optixDenoiserDestroy(denoiser);
    optixDeviceContextDestroy(context);
    std::cout << "OptiX resources cleaned up successfully." << std::endl;
}


void RealTimeDenoiser::denoise(const float* color, const float* albedo, const float* normal, const float* output, int width, int height)
{
    // Denoise the image using OptiX
    std::cout << "Denoising image with OptiX..." << std::endl;

}