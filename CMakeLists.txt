cmake_minimum_required(VERSION 4.1)
project(CarrotGameEngine LANGUAGES CXX C VERSION 0.0.1)
enable_testing()

# ------------------------------------------------------------------------
# Default build type for single-config generators (Ninja, Makefiles)
# ------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ------------------------------------------------------------------------
# Core settings
# ------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Output directories (bin/Debug, bin/Release, etc.)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG Debug Release)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIG})
endforeach()

# ------------------------------------------------------------------------
# Engine Sources
# ------------------------------------------------------------------------
set(CARROT_ENGINE_SOURCES
        # ========================================
        # Engine Core & Root
        # ========================================
        src/Engine/CarrotEngine.h
        src/Engine/Engine.cpp
        src/Engine/Engine.h
        src/Engine/EngineConfig.cpp
        src/Engine/EngineConfig.h

        # ========================================
        # Core Systems
        # ========================================
        src/Engine/Core/Application.cpp
        src/Engine/Core/Application.h
        src/Engine/Core/CoreDefines.h
        src/Engine/Core/EnginePaths.h
        src/Engine/Core/GameContext.h
        src/Engine/Core/GameView.cpp
        src/Engine/Core/GameView.h
        src/Engine/Core/GameRuntime.cpp
        src/Engine/Core/GameRuntime.h
        src/Engine/Core/GameState.cpp
        src/Engine/Core/GameState.h
        src/Engine/Core/LogSink.cpp
        src/Engine/Core/LogSink.h
        src/Engine/Core/Logger.cpp
        src/Engine/Core/Logger.h
        src/Engine/Core/Memory/Arena.cpp
        src/Engine/Core/Memory/Arena.h
        src/Engine/Core/Memory/Ring.cpp
        src/Engine/Core/Memory/Ring.h
        src/Engine/Core/Module.h
        src/Engine/Core/Platform/Platform.h
        src/Engine/Core/Platform/Window.h

        # ========================================
        # Collision
        # ========================================
        src/Engine/Collision/CollisionWorld.cpp
        src/Engine/Collision/CollisionWorld.h

        # ========================================
        # Events & Input
        # ========================================
        src/Engine/Events/Events.h
        src/Engine/Input/KeyCodes.h
        src/Engine/Input/ActionMap.cpp
        src/Engine/Input/ActionMap.h
        src/Engine/Input/GameplayInputRouter.cpp
        src/Engine/Input/GameplayInputRouter.h
        src/Engine/Input/InputActionId.h
        src/Engine/Input/InputActionCatalog.h
        src/Engine/Input/InputBindingStore.cpp
        src/Engine/Input/InputBindingStore.h
        src/Engine/Input/InputRebindSession.cpp
        src/Engine/Input/InputRebindSession.h
        src/Engine/Input/ControllerManager.cpp
        src/Engine/Input/ControllerManager.h
        src/Engine/Input/ControllerState.h
        src/Engine/Input/PlatformKeyMapping.cpp
        src/Engine/Input/PlatformKeyMapping.h

        # ========================================
        # Window, Debug & Hot Reload
        # ========================================
        src/Engine/Window/Window.cpp
        src/Engine/Window/Window.h
        src/Engine/Debug/DebugOverlay.cpp
        src/Engine/Debug/DebugOverlay.h
        src/Engine/HotReload/ShaderWatcher.cpp
        src/Engine/HotReload/ShaderWatcher.h

        # ========================================
        # Renderer (High-level)
        # ========================================
        src/Engine/Renderer/Camera/Camera2D.h
        src/Engine/Renderer/Draw/QuadSamplerPreset.h
        src/Engine/Renderer/Draw/QuadInstanceData.h
        src/Engine/Renderer/Draw/RenderLayer.h
        src/Engine/Renderer/Draw/SpriteDrawInfo.h
        src/Engine/Renderer/Draw/TexturedQuadBatch.h
        src/Engine/Renderer/Draw/TexturedQuadCameraUniform.h
        src/Engine/Renderer/Draw/TexturedQuadTypes.h
        src/Engine/Renderer/Primitives/QuadMesh.h
        src/Engine/Renderer/Primitives/QuadVertex.h
        src/Engine/Renderer/Renderer.cpp
        src/Engine/Renderer/Renderer.h
        src/Engine/Renderer/RendererService.cpp
        src/Engine/Renderer/RendererService.h
        src/Engine/Save/SaveService.cpp
        src/Engine/Save/SaveService.h
        src/Engine/Scene/Scene.cpp
        src/Engine/Scene/Scene.h
        src/Engine/World/AuthoredInteractions.cpp
        src/Engine/World/AuthoredInteractions.h
        src/Engine/World/Components/AuthoredGeometryComponent.h
        src/Engine/World/Components/CollisionComponent.h
        src/Engine/World/Components/SpriteComponent.h
        src/Engine/World/Components/SpriteAnimatorComponent.h
        src/Engine/World/Components/TileObjectComponent.h
        src/Engine/World/Components/TilemapComponent.h
        src/Engine/World/Components/TransformComponent.h
        src/Engine/World/Components/TriggerComponent.h
        src/Engine/World/Components/VisibilityRegionComponent.h
        src/Engine/World/Controllers/InteractionController.cpp
        src/Engine/World/Controllers/InteractionController.h
        src/Engine/World/Controllers/PatrolNpcController.cpp
        src/Engine/World/Controllers/PatrolNpcController.h
        src/Engine/World/Controllers/PlayerController.cpp
        src/Engine/World/Controllers/PlayerController.h
        src/Engine/World/Import/TilemapWorldBridge.cpp
        src/Engine/World/Import/TilemapWorldBridge.h
        src/Engine/World/Motion/MovementBody.cpp
        src/Engine/World/Motion/MovementBody.h
        src/Engine/World/Motion/MovementMotor.h
        src/Engine/World/Motion/MotionTypes.h
        src/Engine/World/Motion/TopDownMovementMotor.cpp
        src/Engine/World/Motion/TopDownMovementMotor.h
        src/Engine/World/Motion.h
        src/Engine/World/SceneContinuity.cpp
        src/Engine/World/SceneContinuity.h
        src/Engine/World/SceneLoader.cpp
        src/Engine/World/SceneLoader.h
        src/Engine/World/WorldComposition.cpp
        src/Engine/World/WorldComposition.h
        src/Engine/World/TriggerQuery.cpp
        src/Engine/World/TriggerQuery.h
        src/Engine/World/World.cpp
        src/Engine/World/World.h
        src/Engine/World/WorldLayering.cpp
        src/Engine/World/WorldLayering.h
        src/Engine/World/WorldObject.h
        src/Engine/World/WorldUnits.h

        # ========================================
        # RHI - Abstract Interface
        # ========================================
        src/Engine/RHI/Buffer.h
        src/Engine/RHI/CommandList.h
        src/Engine/RHI/CommandQueue.h
        src/Engine/RHI/Device.h
        src/Engine/RHI/Fence.h
        src/Engine/RHI/Pipeline.h
        src/Engine/RHI/RHI.cpp
        src/Engine/RHI/RHI.h
        src/Engine/RHI/Sampler.h
        src/Engine/RHI/SamplerPresets.h
        src/Engine/RHI/Semaphore.h
        src/Engine/RHI/Swapchain.h
        src/Engine/RHI/Texture.h
        src/Engine/RHI/Backends/Null/NullRHIContext.cpp
        src/Engine/RHI/Backends/Null/NullRHIContext.h

        # ========================================
        # RHI - Vulkan Backend
        # ========================================
        src/Engine/RHI/Backends/Vulkan/Pipelines/VulkanTexturedQuadPipeline.cpp
        src/Engine/RHI/Backends/Vulkan/Pipelines/VulkanTexturedQuadPipeline.h
        src/Engine/RHI/Backends/Vulkan/Pipelines/VulkanComputePipeline.cpp
        src/Engine/RHI/Backends/Vulkan/Pipelines/VulkanComputePipeline.h
        src/Engine/RHI/Backends/Vulkan/VulkanBuffer.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanBuffer.h
        src/Engine/RHI/Backends/Vulkan/VulkanCommandQueue.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanCommandQueue.h
        src/Engine/RHI/Backends/Vulkan/VulkanCommon.h
        src/Engine/RHI/Backends/Vulkan/VulkanCore.h
        src/Engine/RHI/Backends/Vulkan/VulkanDevice.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanDevice.h
        src/Engine/RHI/Backends/Vulkan/VulkanPipeline.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanPipeline.h
        src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanRHIContext.h
        src/Engine/RHI/Backends/Vulkan/VulkanUploadRing.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanUploadRing.h
        src/Engine/RHI/Backends/Vulkan/VulkanRenderPass.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanRenderPass.h
        src/Engine/RHI/Backends/Vulkan/VulkanSampler.h
        src/Engine/RHI/Backends/Vulkan/VulkanSwapchain.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanSwapchain.h
        src/Engine/RHI/Backends/Vulkan/VulkanTexture.cpp
        src/Engine/RHI/Backends/Vulkan/VulkanTexture.h
        src/Engine/RHI/Backends/Vulkan/VulkanUtils.h

        # ========================================
        # Audio System
        # ========================================
        src/Engine/Audio/Audio.cpp
        src/Engine/Audio/Audio.h
        src/Engine/Audio/AudioModule.cpp
        src/Engine/Audio/AudioModule.h
        src/Engine/Audio/AudioTypes.h
        src/Engine/Audio/Backend/AudioBackend.cpp
        src/Engine/Audio/Backend/AudioBackend.h
        src/Engine/Audio/Backend/Null/NullAudioBackend.cpp
        src/Engine/Audio/Backend/Null/NullAudioBackend.h
        src/Engine/Audio/Core/AudioClock.h
        src/Engine/Audio/Core/AudioCommand.h
        src/Engine/Audio/Core/AudioCommandQueue.h
        src/Engine/Audio/Core/AudioCore.h
        src/Engine/Audio/Core/AudioEngine.cpp
        src/Engine/Audio/Core/AudioEngine.h
        src/Engine/Audio/Core/AudioEvent.h
        src/Engine/Audio/Core/AudioEventQueue.h
        src/Engine/Audio/Core/AudioRingBuffer.h
        src/Engine/Audio/Core/AudioService.cpp
        src/Engine/Audio/Core/AudioService.h
        src/Engine/Audio/DSP/Baxandall.h
        src/Engine/Audio/DSP/BiquadFilter.h
        src/Engine/Audio/DSP/Delay.h
        src/Engine/Audio/DSP/DspUnit.h
        src/Engine/Audio/DSP/Envelope.cpp
        src/Engine/Audio/DSP/Envelope.h
        src/Engine/Audio/DSP/Limiter.h
        src/Engine/Audio/DSP/Pan.h
        src/Engine/Audio/DSP/Resampler.h
        src/Engine/Audio/DSP/Saturator.h
        src/Engine/Audio/DSP/SchroederReverb.h
        src/Engine/Audio/DSP/VcaBusComp.h
        src/Engine/Audio/Mixer/AudioBus.h
        src/Engine/Audio/Mixer/AudioMixer.cpp
        src/Engine/Audio/Mixer/AudioMixer.h
        src/Engine/Audio/Mixer/AudioRouting.h
        src/Engine/Audio/Mixer/FxChain.h
        src/Engine/Audio/Sample/AudioSample.h
        src/Engine/Audio/Sample/WavCore.h
        src/Engine/Audio/Sample/WavLoader.cpp
        src/Engine/Audio/Sample/WavLoader.h
        src/Engine/Audio/Streaming/AudioStream.h
        src/Engine/Audio/Streaming/TestStreamDecoder.cpp
        src/Engine/Audio/Streaming/TestStreamDecoder.h
        src/Engine/Audio/Streaming/WavStreamDecoder.cpp
        src/Engine/Audio/Streaming/WavStreamDecoder.h
        src/Engine/Audio/Voice/Voice.h
        src/Engine/Audio/Voice/VoiceHandle.h

        # ========================================
        # Asset System
        # ========================================
        src/Engine/Assets/AssetDatabase.h
        src/Engine/Assets/AssetDiscovery.cpp
        src/Engine/Assets/AssetDiscovery.h
        src/Engine/Assets/AssetHandle.h
        src/Engine/Assets/AssetID.h
        src/Engine/Assets/AssetIteration.h
        src/Engine/Assets/ImportedAssetCache.cpp
        src/Engine/Assets/ImportedAssetCache.h
        src/Engine/Assets/AssetManager.cpp
        src/Engine/Assets/AssetManager.h
        src/Engine/Assets/AssetRegistry.h
        src/Engine/Assets/AssetService.cpp
        src/Engine/Assets/AssetService.h
        src/Engine/Assets/Audio/AudioAsset.h
        src/Engine/Assets/Audio/AudioAssetLoader.cpp
        src/Engine/Assets/Audio/AudioAssetLoader.h
        src/Engine/Assets/Audio/CookedAudio.cpp
        src/Engine/Assets/Audio/CookedAudio.h
        src/Engine/Assets/Audio/AudioAssetManifestImporter.cpp
        src/Engine/Assets/Audio/AudioAssetManifestImporter.h
        src/Engine/Assets/Audio/AudioAssetRegistry.cpp
        src/Engine/Assets/Audio/AudioAssetRegistry.h
        src/Engine/Assets/Audio/AudioAssetSystem.cpp
        src/Engine/Assets/Audio/AudioAssetSystem.h
        src/Engine/Assets/Font/FontAsset.h
        src/Engine/Assets/Font/CookedFont.cpp
        src/Engine/Assets/Font/CookedFont.h
        src/Engine/Assets/Font/FontAssetLoader.cpp
        src/Engine/Assets/Font/FontAssetLoader.h
        src/Engine/Assets/Font/MSDFAtlasWrapper.cpp
        src/Engine/Assets/Font/MSDFAtlasWrapper.h
        src/Engine/Assets/Font/FontAssetManifestImporter.cpp
        src/Engine/Assets/Font/FontAssetManifestImporter.h
        src/Engine/Assets/Font/FontAssetRegistry.cpp
        src/Engine/Assets/Font/FontAssetRegistry.h
        src/Engine/Assets/Font/FontAssetSystem.cpp
        src/Engine/Assets/Font/FontAssetSystem.h
        src/Engine/Assets/Font/TextLayout.cpp
        src/Engine/Assets/Font/TextLayout.h
        src/Engine/Assets/Image/ImageAsset.h
        src/Engine/Assets/Image/ImageAssetImporter.cpp
        src/Engine/Assets/Image/ImageAssetImporter.h
        src/Engine/Assets/Image/ImageLoadError.cpp
        src/Engine/Assets/Image/ImageLoadError.h
        src/Engine/Assets/Scene/SceneAsset.h
        src/Engine/Assets/Scene/SceneAssetManifestImporter.cpp
        src/Engine/Assets/Scene/SceneAssetManifestImporter.h
        src/Engine/Assets/Scene/SceneAssetRegistry.cpp
        src/Engine/Assets/Scene/SceneAssetRegistry.h
        src/Engine/Assets/Scene/SceneAssetSystem.cpp
        src/Engine/Assets/Scene/SceneAssetSystem.h
        src/Engine/Assets/Shaders/ShaderFileProvider.h
        src/Engine/Assets/Shaders/VFSShaderFileProvider.h
        src/Engine/Assets/Sprite/AsepriteSpriteAssetImporter.cpp
        src/Engine/Assets/Sprite/AsepriteSpriteAssetImporter.h
        src/Engine/Assets/Sprite/CookedSprite.cpp
        src/Engine/Assets/Sprite/CookedSprite.h
        src/Engine/Assets/Sprite/LoadedSpriteAsset.cpp
        src/Engine/Assets/Sprite/LoadedSpriteAsset.h
        src/Engine/Assets/Sprite/NativeSpriteAssetImporter.cpp
        src/Engine/Assets/Sprite/NativeSpriteAssetImporter.h
        src/Engine/Assets/Sprite/SpriteAnimation.h
        src/Engine/Assets/Sprite/SpriteAnimator.cpp
        src/Engine/Assets/Sprite/SpriteAnimator.h
        src/Engine/Assets/Sprite/SpriteAsset.cpp
        src/Engine/Assets/Sprite/SpriteAsset.h
        src/Engine/Assets/Sprite/SpriteAssetLoader.cpp
        src/Engine/Assets/Sprite/SpriteAssetLoader.h
        src/Engine/Assets/Sprite/SpriteAssetManifestImporter.cpp
        src/Engine/Assets/Sprite/SpriteAssetManifestImporter.h
        src/Engine/Assets/Sprite/SpriteAssetRegistry.cpp
        src/Engine/Assets/Sprite/SpriteAssetRegistry.h
        src/Engine/Assets/Sprite/SpriteAssetSystem.cpp
        src/Engine/Assets/Sprite/SpriteAssetSystem.h
        src/Engine/Assets/Sprite/SpriteFrame.h
        src/Engine/Assets/Tilemap/LoadedTilemapAsset.cpp
        src/Engine/Assets/Tilemap/LoadedTilemapAsset.h
        src/Engine/Assets/Tilemap/CookedTilemap.cpp
        src/Engine/Assets/Tilemap/CookedTilemap.h
        src/Engine/Assets/Tilemap/NativeTilemapAssetImporter.cpp
        src/Engine/Assets/Tilemap/NativeTilemapAssetImporter.h
        src/Engine/Assets/Tilemap/TiledTilemapAssetImporter.cpp
        src/Engine/Assets/Tilemap/TiledTilemapAssetImporter.h
        src/Engine/Assets/Tilemap/TiledWorld.cpp
        src/Engine/Assets/Tilemap/TiledWorld.h
        src/Engine/Assets/Tilemap/TilemapAsset.h
        src/Engine/Assets/Tilemap/TilemapAssetLoader.cpp
        src/Engine/Assets/Tilemap/TilemapAssetLoader.h
        src/Engine/Assets/Tilemap/TilemapAssetManifestImporter.cpp
        src/Engine/Assets/Tilemap/TilemapAssetManifestImporter.h
        src/Engine/Assets/Tilemap/TilemapAssetRegistry.cpp
        src/Engine/Assets/Tilemap/TilemapAssetRegistry.h
        src/Engine/Assets/Tilemap/TilemapAssetSystem.cpp
        src/Engine/Assets/Tilemap/TilemapAssetSystem.h
        src/Engine/Assets/Tilemap/TilemapValidation.cpp
        src/Engine/Assets/Tilemap/TilemapValidation.h
        src/Engine/Assets/Tilemap/TypedObjectConventions.cpp
        src/Engine/Assets/Tilemap/TypedObjectConventions.h
        src/Engine/Assets/Texture/TextureAsset.h
        src/Engine/Assets/Texture/CookedTexture.cpp
        src/Engine/Assets/Texture/CookedTexture.h
        src/Engine/Assets/Texture/TextureAssetLoader.cpp
        src/Engine/Assets/Texture/TextureAssetLoader.h
        src/Engine/Assets/Texture/TextureAssetManifestImporter.cpp
        src/Engine/Assets/Texture/TextureAssetManifestImporter.h
        src/Engine/Assets/Texture/TextureAssetRegistry.cpp
        src/Engine/Assets/Texture/TextureAssetRegistry.h
        src/Engine/Assets/Texture/TextureAssetSystem.cpp
        src/Engine/Assets/Texture/TextureAssetSystem.h

        # ========================================
        # Utilities
        # ========================================
        src/Engine/Utils/Assert.h
        src/Engine/Utils/DebugBreak.h
        src/Engine/Utils/File/FileUtils.cpp
        src/Engine/Utils/File/FileUtils.h
        src/Engine/Utils/File/PlatformPaths.h
        src/Engine/Utils/JSON/Private/JsonAST.h
        src/Engine/Utils/JSON/Private/JsonArrayView.cpp
        src/Engine/Utils/JSON/Private/JsonDocument.cpp
        src/Engine/Utils/JSON/Private/JsonLexer.cpp
        src/Engine/Utils/JSON/Private/JsonLexer.h
        src/Engine/Utils/JSON/Private/JsonObjectView.cpp
        src/Engine/Utils/JSON/Private/JsonParser.cpp
        src/Engine/Utils/JSON/Private/JsonParser.h
        src/Engine/Utils/JSON/Private/JsonToken.h
        src/Engine/Utils/JSON/Private/JsonValueView.cpp
        src/Engine/Utils/JSON/Private/JsonWriter.cpp
        src/Engine/Utils/JSON/Public/JsonArrayView.h
        src/Engine/Utils/JSON/Public/JsonDocument.h
        src/Engine/Utils/JSON/Public/JsonObjectView.h
        src/Engine/Utils/JSON/Public/JsonValueView.h
        src/Engine/Utils/JSON/Public/JsonWriter.h
        src/Engine/Utils/MulticastDelegate.h
        src/Engine/Utils/ShaderUtils.cpp
        src/Engine/Utils/ShaderUtils.h
        src/Engine/Utils/TextEncoding.cpp
        src/Engine/Utils/TextEncoding.h

        # ========================================
        # IO & Virtual File System
        # ========================================
        src/Engine/IO/VirtualFileSystem.cpp
        src/Engine/IO/VirtualFileSystem.h

        # ========================================
        # Shaders
        # ========================================
        shaders/ShaderCommon.h
        src/Engine/UI/UIModule.cpp
        src/Engine/UI/UIModule.h
        src/Engine/UI/UIInputOwnership.h
        src/Engine/UI/UIWidget.cpp
        src/Engine/UI/UIWidget.h
        src/Engine/UI/UIRootWidget.h
        src/Engine/UI/UIService.cpp
        src/Engine/UI/UIService.h
        src/Engine/UI/Widgets/UIButton.cpp
        src/Engine/UI/Widgets/UIButton.h
        src/Engine/UI/Widgets/UIAssetPreview.cpp
        src/Engine/UI/Widgets/UIAssetPreview.h
        src/Engine/UI/Widgets/UIFocusScope.cpp
        src/Engine/UI/Widgets/UIFocusScope.h
        src/Engine/UI/Widgets/UILabel.cpp
        src/Engine/UI/Widgets/UILabel.h
        src/Engine/UI/Widgets/UIPanel.cpp
        src/Engine/UI/Widgets/UIPanel.h
        src/Engine/UI/Widgets/UIStack.cpp
        src/Engine/UI/Widgets/UIStack.h
        src/Engine/UI/UI.h
)

# ------------------------------------------------------------------------
# Platform / Windowing / RHI specific sources
# ------------------------------------------------------------------------
if (APPLE)
    enable_language(OBJCXX)

    set(CMAKE_OBJCXX_STANDARD 23)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)

    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/Input/AppleControllerManager.mm

            src/Engine/HotReload/ShaderWatcherApple.mm

            src/Engine/Core/Platform/Cocoa/CocoaWindow.mm
            src/Engine/Core/Platform/Cocoa/CocoaWindow.h
            src/Engine/Core/Platform/Cocoa/AppDelegate.mm
            src/Engine/Core/Platform/Cocoa/AppDelegate.h
            src/Engine/Core/Platform/Cocoa/CocoaInputUtils.h
            src/Engine/Core/Platform/Cocoa/CocoaContentView.mm
            src/Engine/Core/Platform/Cocoa/CocoaContentView.h

            src/Engine/RHI/Backends/Metal/MetalCommon.h
            src/Engine/RHI/Backends/Metal/MetalCore.h
            src/Engine/RHI/Backends/Metal/MetalRHIContext.cpp
            src/Engine/RHI/Backends/Metal/MetalRHIContext.h
            src/Engine/RHI/Backends/Metal/MetalDevice.cpp
            src/Engine/RHI/Backends/Metal/MetalDevice.h
            src/Engine/RHI/Backends/Metal/MetalCommandQueue.cpp
            src/Engine/RHI/Backends/Metal/MetalCommandQueue.h
            src/Engine/RHI/Backends/Metal/MetalUploadRing.cpp
            src/Engine/RHI/Backends/Metal/MetalUploadRing.h
            src/Engine/RHI/Backends/Metal/MetalLayerBridge.mm
            src/Engine/RHI/Backends/Metal/MetalLayerBridge.h
            src/Engine/RHI/Backends/Metal/MetalSwapchain.cpp
            src/Engine/RHI/Backends/Metal/MetalSwapchain.h
            src/Engine/RHI/Backends/Metal/MetalRenderEncoder.cpp
            src/Engine/RHI/Backends/Metal/MetalRenderEncoder.h
            src/Engine/RHI/Backends/Metal/MetalBuffer.cpp
            src/Engine/RHI/Backends/Metal/MetalBuffer.h
            src/Engine/RHI/Backends/Metal/MetalTexture.cpp
            src/Engine/RHI/Backends/Metal/MetalTexture.h
            src/Engine/RHI/Backends/Metal/Pipelines/MetalTexturedQuadPipeline.cpp
            src/Engine/RHI/Backends/Metal/Pipelines/MetalTexturedQuadPipeline.h
            src/Engine/RHI/Backends/Metal/Pipelines/MetalComputePipeline.cpp
            src/Engine/RHI/Backends/Metal/Pipelines/MetalComputePipeline.h
            src/Engine/RHI/Backends/Metal/MetalUtils.h
            src/Engine/RHI/Backends/Metal/MetalDescriptorEncoding.h
            src/Engine/RHI/Backends/Metal/MetalSampler.h

            src/Engine/Audio/Backend/Apple/AppleAudioBackend.cpp
            src/Engine/Audio/Backend/Apple/AppleAudioBackend.h

            src/Engine/Utils/File/ApplePlatformPaths.cpp
    )
elseif (WIN32)
    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/Input/WindowsControllerManager.cpp

            src/Engine/HotReload/ShaderWatcherWindows.cpp

            src/Engine/Core/Platform/Win32/Win32Window.cpp
            src/Engine/Core/Platform/Win32/Win32Window.h

            src/Engine/RHI/Backends/DirectX12/DirectX12Common.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Core.h
            src/Engine/RHI/Backends/DirectX12/DirectX12RHIContext.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12RHIContext.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Device.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Device.h
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandQueue.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandQueue.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Swapchain.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Swapchain.h
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandList.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12CommandList.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Fence.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Fence.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Sampler.h
            src/Engine/RHI/Backends/DirectX12/DirectX12UploadRing.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12UploadRing.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Buffer.cpp
            src/Engine/RHI/Backends/DirectX12/DirectX12Buffer.h
            src/Engine/RHI/Backends/DirectX12/DirectX12Texture.h
            src/Engine/RHI/Backends/DirectX12/Pipelines/DirectX12ComputePipeline.cpp
            src/Engine/RHI/Backends/DirectX12/Pipelines/DirectX12ComputePipeline.h
            src/Engine/RHI/Backends/DirectX12/Pipelines/DirectX12TexturedQuadPipeline.cpp
            src/Engine/RHI/Backends/DirectX12/Pipelines/DirectX12TexturedQuadPipeline.h

            src/Engine/Audio/Backend/Windows/WindowsAudioBackend.cpp
            src/Engine/Audio/Backend/Windows/WindowsAudioBackend.h

            src/Engine/Utils/File/WindowsPlatformPaths.cpp
    )
elseif (UNIX)
    # Define the cache variable with a proper default value right away.
    # This runs only on first configure (or if deleted from cache).
    # After that, user choices (-D or cmake-gui) are preserved.
    set(CARROT_LINUX_WINDOWING "Auto" CACHE STRING
            "Linux windowing system to target: Auto (prefer Wayland), Wayland, or X11")

    # Restrict the possible values in cmake-gui / ccmake (optional but very nice)
    set_property(CACHE CARROT_LINUX_WINDOWING PROPERTY STRINGS "Auto" "Wayland" "X11")

    # Now resolve which backend we're actually using
    if (CARROT_LINUX_WINDOWING STREQUAL "Auto")
        find_package(PkgConfig QUIET)
        if (PkgConfig_FOUND)
            pkg_check_modules(WL_CLIENT QUIET wayland-client)
            pkg_check_modules(WL_EGL QUIET wayland-egl)
            pkg_check_modules(XKB QUIET xkbcommon)
            pkg_check_modules(X11_PKG QUIET x11)

            if (WL_CLIENT_FOUND AND WL_EGL_FOUND AND XKB_FOUND)
                set(_backend Wayland)
                message(STATUS "Auto: Wayland selected (dev packages detected)")
            elseif (X11_PKG_FOUND)
                set(_backend X11)
                message(STATUS "Auto: X11 selected (dev packages detected)")
            else ()
                message(FATAL_ERROR
                        "Linux auto-detection found no supported windowing backend. "
                        "Install Wayland development packages (wayland-client, wayland-egl, xkbcommon) "
                        "or X11 development packages (x11).")
            endif ()
        else ()
            message(FATAL_ERROR
                    "pkg-config is required to detect the supported Linux windowing backend.")
        endif ()
    else ()
        set(_backend ${CARROT_LINUX_WINDOWING})
    endif ()

    # Apply the final choice
    if (_backend STREQUAL "Wayland")
        set(CARROT_PLATFORM_WAYLAND ON CACHE INTERNAL "")
        add_compile_definitions(CARROT_PLATFORM_WAYLAND=1)
    elseif (_backend STREQUAL "X11")
        set(CARROT_PLATFORM_X11 ON CACHE INTERNAL "")
        add_compile_definitions(CARROT_PLATFORM_X11=1)
    else ()
        message(FATAL_ERROR "Internal error: unexpected backend ${_backend}")
    endif ()

    message(STATUS "Linux windowing backend: ${_backend}")

    if (CARROT_PLATFORM_WAYLAND)
        list(APPEND CARROT_ENGINE_SOURCES
                src/Engine/Core/Platform/Wayland/WaylandWindow.cpp
                src/Engine/Core/Platform/Wayland/WaylandWindow.h
                src/Engine/Core/Platform/Wayland/Protocols/xdg-shell-client-protocol.c
                src/Engine/Core/Platform/Wayland/Protocols/xdg-shell-client-protocol.h
                src/Engine/Core/Platform/Wayland/Protocols/xdg-decoration-unstable-v1-protocol.c
                src/Engine/Core/Platform/Wayland/Protocols/xdg-decoration-unstable-v1-client-protocol.h
        )
    endif ()

    if (CARROT_PLATFORM_X11)
        list(APPEND CARROT_ENGINE_SOURCES
                src/Engine/Core/Platform/X11/X11Window.cpp
                src/Engine/Core/Platform/X11/X11Window.h
        )
    endif ()

    # All Linux platforms get these
    list(APPEND CARROT_ENGINE_SOURCES
            src/Engine/Input/LinuxControllerManager.cpp

            src/Engine/HotReload/ShaderWatcherLinux.cpp

            src/Engine/Audio/Backend/Linux/LinuxAudioBackend.cpp
            src/Engine/Audio/Backend/Linux/LinuxAudioBackend.h

            src/Engine/Utils/File/LinuxPlatformPaths.cpp
    )

endif()

# ------------------------------------------------------------------------
# External Dependencies
# ------------------------------------------------------------------------
add_subdirectory(deps/CarrotPNG)

set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
if(WIN32)
    # Match the default Windows/vcpkg CRT selection used by the rest of the build.
    set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
endif()
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
add_subdirectory(deps/msdf-atlas-gen)

# msdf-atlas-gen and msdfgen publish their own include directories through
# linked target interfaces. Mark those dependency targets as system so their
# transitive headers are emitted as -isystem/-external:I for consumers.
set_property(TARGET msdf-atlas-gen PROPERTY SYSTEM ON)
set_property(TARGET msdfgen-core PROPERTY SYSTEM ON)
if(TARGET msdfgen-ext)
    set_property(TARGET msdfgen-ext PROPERTY SYSTEM ON)
endif()

# ------------------------------------------------------------------------
# Engine Static Library
# ------------------------------------------------------------------------
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${CARROT_ENGINE_SOURCES})

add_library(CarrotEngine STATIC ${CARROT_ENGINE_SOURCES})

target_compile_definitions(CarrotEngine PRIVATE
        CARROT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

target_include_directories(CarrotEngine PRIVATE
        ${CMAKE_BINARY_DIR}/src/Engine/HotReload/Config
)

target_link_libraries(CarrotEngine PRIVATE
        CarrotPNG
        msdf-atlas-gen::msdf-atlas-gen
)

# ------------------------------------------------------------------------
# Debug/Release macros & optimizations
# ------------------------------------------------------------------------
target_compile_definitions(CarrotEngine PUBLIC
        $<$<CONFIG:Debug>:_DEBUG CARROT_ENABLE_TRACY>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_compile_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:MSVC>>>:-O0 -g>
        $<$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:MSVC>>>:-O3>
        $<$<AND:$<CONFIG:RelWithDebInfo>,$<NOT:$<BOOL:MSVC>>>:-O2 -g>
        $<$<AND:$<CONFIG:MinSizeRel>,$<NOT:$<BOOL:MSVC>>>:-Os>
)

# ------------------------------------------------------------------------
# Include directories – split user vs. third-party
# ------------------------------------------------------------------------
# project code -> full warnings
target_include_directories(CarrotEngine PUBLIC
        src/Engine
)

# Everything under deps/ is third-party -> treat as system headers
target_include_directories(CarrotEngine SYSTEM PUBLIC
        deps
        deps/CarrotHLM/include
        deps/spdlog/include
        deps/tracy/public
        ${Vulkan_INCLUDE_DIRS}
        deps/metal-cpp
        deps/metal-cpp-extensions
)

target_precompile_headers(CarrotEngine PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Core/Pch.h>"
)

# ------------------------------------------------------------------------
# Warnings & sanitizers
# ------------------------------------------------------------------------
target_compile_options(CarrotEngine PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
        -Wall -Wextra -Werror=return-type -Wshadow -Wno-missing-braces
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /permissive- /wd4201
        >
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
    target_compile_options(CarrotEngine PRIVATE
            -Wno-padded /wd4820
            -Wno-unique-object-duplication
            -Wno-exit-time-destructors -Wno-global-constructors
            -Wno-switch-enum -Wno-switch-default
            -Wno-nrvo
            -Wno-covered-switch-default
            -Wno-language-extension-token
            -Wno-disabled-macro-expansion
            -Wno-nested-anon-types
            -Wno-unsafe-buffer-usage
            -Wno-unsafe-buffer-usage-in-libc-call
            -Wno-documentation
    )
endif()



target_compile_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:MSVC>>,$<NOT:$<BOOL:APPLE>>>:-fsanitize=address,undefined -fno-omit-frame-pointer>
)

target_link_options(CarrotEngine PUBLIC
        $<$<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:MSVC>>,$<NOT:$<BOOL:APPLE>>>:-fsanitize=address,undefined>
)

# ------------------------------------------------------------------------
# macOS
# ------------------------------------------------------------------------
if(APPLE)
    find_package(Vulkan REQUIRED)
    target_link_libraries(CarrotEngine PRIVATE Vulkan::Vulkan)

    set(CARROT_PLATFORM_COCOA ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_COCOA=1)

    target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_METAL_EXT)

    target_link_libraries(CarrotEngine PRIVATE
            "-framework Metal"
            "-framework MetalKit"
            "-framework GameController"
            "-framework Cocoa"
            "-framework AppKit"
            "-framework QuartzCore"
            "-framework CoreServices"
            "-framework CoreAudio"
            "-framework AudioUnit"
            "-framework AudioToolbox"
    )

    # Set source file that will have the defines applied to them, which must also include all metal-cpp headers
    set(METAL_CPP_PRIVATE_IMPL src/Engine/RHI/Backends/Metal/MetalRHIContext.cpp)

    # Make this file contain the five PRIVATE_IMPLEMENTATION defines
    set_source_files_properties(${METAL_CPP_PRIVATE_IMPL}
            PROPERTIES COMPILE_DEFINITIONS
            "NS_PRIVATE_IMPLEMENTATION;CA_PRIVATE_IMPLEMENTATION;MTL_PRIVATE_IMPLEMENTATION;MTK_PRIVATE_IMPLEMENTATION;APPKIT_PRIVATE_IMPLEMENTATION"
    )
# ------------------------------------------------------------------------
# Windows
# ------------------------------------------------------------------------
elseif(WIN32)
    find_package(Vulkan REQUIRED)
    target_link_libraries(CarrotEngine PRIVATE Vulkan::Vulkan)

    set(CARROT_PLATFORM_WIN32 ON CACHE INTERNAL "")
    add_compile_definitions(CARROT_PLATFORM_WIN32=1)

    target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_WIN32_KHR)

    target_link_libraries(CarrotEngine PRIVATE d3d12 dxgi dxguid ole32 avrt uuid gameinput)
    target_compile_definitions(CarrotEngine PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
# ------------------------------------------------------------------------
# Linux
# ------------------------------------------------------------------------
elseif(UNIX)
    # Find Vulkan
    find_package(Vulkan REQUIRED)
    find_package(ALSA REQUIRED)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(PULSEAUDIO REQUIRED IMPORTED_TARGET
            libpulse-simple
            libpulse
    )
    pkg_check_modules(LIBUDEV REQUIRED IMPORTED_TARGET libudev)
    pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
    pkg_check_modules(LIBDECOR QUIET IMPORTED_TARGET libdecor-0)

    if (CARROT_PLATFORM_X11)
        find_package(X11 REQUIRED)
    endif ()

    # Apply libc++ only to C++ compilation and linking
    target_compile_options(CarrotEngine PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
    )
    target_link_options(CarrotEngine PUBLIC
            $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
    )

    # Apply libc++ to CarrotPNG
    target_compile_options(CarrotPNG PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
    )
    target_link_options(CarrotPNG PUBLIC
            $<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>
    )

    if (CARROT_PLATFORM_WAYLAND)
        target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_WAYLAND_KHR)
    elseif (CARROT_PLATFORM_X11)
        target_compile_definitions(CarrotEngine PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    endif ()

    if (LIBDECOR_FOUND)
        target_compile_definitions(CarrotEngine PUBLIC CARROT_HAS_LIBDECOR=1)
    endif ()

    target_link_libraries(CarrotEngine PRIVATE
            Vulkan::Vulkan
            ALSA::ALSA
            PkgConfig::PULSEAUDIO
            PkgConfig::LIBUDEV
            PkgConfig::LIBEVDEV
            dl
            pthread
    )

    if (CARROT_PLATFORM_WAYLAND)
        target_link_libraries(CarrotEngine PRIVATE
                wayland-client
                wayland-cursor
                wayland-egl
                xkbcommon
        )
    elseif (CARROT_PLATFORM_X11)
        target_link_libraries(CarrotEngine PRIVATE X11::X11)
    endif ()

    if (LIBDECOR_FOUND)
        target_link_libraries(CarrotEngine PRIVATE PkgConfig::LIBDECOR)
    endif ()
endif()

# ------------------------------------------------------------------------
# Sandbox / Game Executable
# ------------------------------------------------------------------------
set(CARROT_SANDBOX_SOURCES
        src/Sandbox/main.cpp
        src/Sandbox/Sandbox.cpp
        src/Sandbox/Sandbox.h
        src/Sandbox/GameplayState.cpp
        src/Sandbox/GameplayState.h
        src/Sandbox/SandboxGame.cpp
        src/Sandbox/SandboxGame.h
        src/Sandbox/SandboxInputConfig.h
        src/Sandbox/GameplayRuntimeState.cpp
        src/Sandbox/GameplayRuntimeState.h
)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${CARROT_SANDBOX_SOURCES})

add_executable(CarrotSandbox ${CARROT_SANDBOX_SOURCES})  # Rename if you prefer "Game" or "Sandbox"
target_include_directories(CarrotSandbox PRIVATE src/Sandbox)
target_link_libraries(CarrotSandbox PRIVATE CarrotEngine)

set(CARROT_TEST_SOURCES
        tests/TestMain.cpp
        tests/ActionMapTests.cpp
        tests/AssetCookedPipelineTests.cpp
        tests/AssetIterationTests.cpp
        tests/ControllerTests.cpp
        tests/CollisionWorldTests.cpp
        tests/FontAssetTests.cpp
        tests/ForwardPlusDataTests.cpp
        tests/JsonWriterTests.cpp
        tests/RHIBufferTests.cpp
        tests/RHIComputeTests.cpp
        tests/SaveServiceTests.cpp
        tests/SceneLoadingTests.cpp
        tests/TiledWorldTests.cpp
        tests/UITests.cpp
        tests/UILayoutTests.cpp
        tests/UINavigationTests.cpp
        tests/WorldCompositionTests.cpp
        src/Sandbox/GameplayState.cpp
        src/Sandbox/GameplayRuntimeState.cpp
        tests/WindowSystemTests.cpp
)

add_executable(CarrotEngineTests ${CARROT_TEST_SOURCES})
target_include_directories(CarrotEngineTests PRIVATE src/Sandbox tests)
target_link_libraries(CarrotEngineTests PRIVATE CarrotEngine)
target_compile_definitions(CarrotEngineTests PRIVATE CARROT_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME CarrotEngineTests COMMAND CarrotEngineTests)

set(CARROT_EDITOR_SOURCES
        src/Editor/EditorApp.cpp
        src/Editor/EditorApp.h
        src/Editor/main.cpp
)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${CARROT_EDITOR_SOURCES})

add_executable(CarrotEditor ${CARROT_EDITOR_SOURCES})
target_include_directories(CarrotEditor PRIVATE src/Editor)
target_link_libraries(CarrotEditor PRIVATE CarrotEngine)

#if(WIN32)
#    # Use Windows GUI subsystem → no console window
#    target_link_options(CarrotSandbox PRIVATE "/SUBSYSTEM:WINDOWS")
#
#    # Optional but often needed with clang-cl to avoid entry-point mismatch warnings
#    # This tells linker to look for main() even in WINDOWS subsystem (CRT shim helps)
#   target_link_options(CarrotSandbox PRIVATE "/ENTRY:mainCRTStartup")
#endif()

# ------------------------------------------------------------------------
# Shader compilation
# ------------------------------------------------------------------------
include(CompileShaders.cmake)

file(GLOB HLSL_SOURCES "shaders/*.vert.hlsl" "shaders/*.frag.hlsl" "shaders/*.comp.hlsl")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")

# Platform-native shaders
if(APPLE)
    foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
        file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
        compile_hlsl_to_metallib(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}/metal")
    endforeach()
elseif(WIN32)
    foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
        file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
        compile_hlsl_to_dxil(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}/dx12")
    endforeach ()
endif()

# Vulkan shaders for all platforms
foreach(SHADER_FILE IN LISTS HLSL_SOURCES)
    file(RELATIVE_PATH REL_SHADER "${CMAKE_CURRENT_SOURCE_DIR}" "${SHADER_FILE}")
    compile_hlsl_to_spirv(CarrotSandbox "${REL_SHADER}" "${SHADER_OUTPUT_DIR}/vulkan")
endforeach()

# ------------------------------------------------------------------------
# Output
# ------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Platform summary:")
message(STATUS "  Windows        : ${CARROT_PLATFORM_WIN32}")
message(STATUS "  macOS/Cocoa    : ${CARROT_PLATFORM_COCOA}")
message(STATUS "  Linux/Wayland  : ${CARROT_PLATFORM_WAYLAND}")
message(STATUS "  Linux/X11      : ${CARROT_PLATFORM_X11}")
