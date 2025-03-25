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
    // OptixDenoiserSizes denoiser_sizes;
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

    OptixImage2D inputLayer[3] = {};

    inputLayer[0].data = reinterpret_cast<CUdeviceptr>(color);
    inputLayer[0].width = width;
    inputLayer[0].height = height;
    inputLayer[0].rowStrideInBytes = width * sizeof(float) * 3;
    inputLayer[0].pixelStrideInBytes = sizeof(float) * 3;
    inputLayer[0].format = OPTIX_PIXEL_FORMAT_FLOAT3;

    // inputLayer[1].data = reinterpret_cast<CUdeviceptr>(albedo);
    // inputLayer[1].width = width;
    // inputLayer[1].height = height;
    // inputLayer[1].rowStrideInBytes = width * sizeof(float) * 4;
    // inputLayer[1].pixelStrideInBytes = sizeof(float) * 4;
    // inputLayer[1].format = OPTIX_PIXEL_FORMAT_FLOAT4;

    // inputLayer[2].data = reinterpret_cast<CUdeviceptr>(normal);
    // inputLayer[2].width = width;
    // inputLayer[2].height = height;
    // inputLayer[2].rowStrideInBytes = width * sizeof(float) * 4;
    // inputLayer[2].pixelStrideInBytes = sizeof(float) * 4;
    // inputLayer[2].format = OPTIX_PIXEL_FORMAT_FLOAT4;

    OptixImage2D outputLayer = {};
    outputLayer.data = reinterpret_cast<CUdeviceptr>(output);
    outputLayer.width = width;
    outputLayer.height = height;
    outputLayer.rowStrideInBytes = width * sizeof(float) * 3;
    outputLayer.pixelStrideInBytes = sizeof(float) * 3;
    outputLayer.format = OPTIX_PIXEL_FORMAT_FLOAT3;

    OptixDenoiserParams params = {};
    params.hdrIntensity = 0.0;
    params.blendFactor = 0.0;

    OptixDenoiserGuideLayer guideLayer = {};
    // guideLayer.albedo = inputLayer[1];
    // guideLayer.normal = inputLayer[2];
    // guideLayer.albedo = 0;
    // guideLayer.normal = 0;

    OptixDenoiserLayer denoiserLayer = {};
    denoiserLayer.input = inputLayer[0];
    denoiserLayer.output = outputLayer;

    optixDenoiserInvoke(denoiser, 
        nullptr, 
        &params, 
        reinterpret_cast<CUdeviceptr>(denoiser_state), 
        denoiser_sizes.stateSizeInBytes, 
        &guideLayer, 
        &denoiserLayer, 1, 0, 0, 
        reinterpret_cast<CUdeviceptr>(denoiser_scratch), 
        denoiser_sizes.withoutOverlapScratchSizeInBytes
);

    std::cout << "Image denoised successfully." << std::endl;

}