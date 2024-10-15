// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_loader.h>
#include <vk_descriptors.h>
#include <camera.h>



struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;
	Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};


//Hard coded push constant format for compute shaders, allows for 16 floats the shader can access
struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct DeletionQueue
{
	/*
		std::function is innefficient, if you need to delete thousands of objects and want them deleted faster,
		a better implementation would be to store arrays of vulkan handles of various types such as
		VkImage, VkBuffer, and so on. And then delete those from a loop
	*/
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};


struct EngineStats {
	float frametime;
	int triangle_count;
	int drawcall_count;
	float scene_update_time;
	float mesh_draw_time;
};


struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore,		//used so that our render commands wait on the swapchain image request
		_renderSemaphore;			//used to control presenting the image to the OS once the drawing finishes
	VkFence _renderFence;					//lets us wait for the draw commands of a given frame to be finished
	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2;


// For the material system, we are going to hardcode into 2 pipelines, 
// GLTF PBR Opaque, and GLTF PBR Transparent. They all use the same pair of vertex and fragment shader. 
// We will be using 2 descriptor sets only
// https://vkguide.dev/docs/new_chapter_4/engine_arch/
struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		// padding, we need it anyway for uniform buffers
		// In vulkan, when you want to bind a uniform buffer, it needs to meet a minimum requirement for its alignment. 
		// 256 bytes is a good default alignment for this which all the gpus we target meet, 
		// so we are adding those vec4s to pad the structure to 256 bytes
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedImage normalImage;
		VkSampler normalSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	// compiles the pipelines
	void build_pipelines(VulkanEngine* engine);
	// deletes everything
	void clear_resources(VkDevice device);
	// where we will create the descriptor set and return a fully built MaterialInstance struct we can then use when rendering
	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

//Global visibility function to check if a render object is visible within view of our frustum
bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);

class VulkanEngine {
public:
	FrameData _frames[FRAME_OVERLAP];


	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	DeletionQueue _mainDeletionQueue;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;


	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	VmaAllocator _allocator;

	Camera mainCamera;

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();


	//draw loop
	void draw();

	void draw_background(VkCommandBuffer cmd);

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void draw_geometry(VkCommandBuffer cmd);

	//run main loop
	void run();


	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

	void update_scene();

	//Create buffer for vertex geometry buffer
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	//Vulkan Initialize Vars
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	//Vulkan Swapchain Vars
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;					//Format that the swapchain images use when rendering to them
	std::vector<VkImage> _swapchainImages;			//Images are the actual texture to render into
	std::vector<VkImageView> _swapchainImageViews;	//Image views are wrappers for images, allows us to change color, etc.
	VkExtent2D _swapchainExtent;

	//Draw resources
	AllocatedImage _drawImage;
	VkExtent2D _drawExtent;
	AllocatedImage _depthImage;

	//We will be storing one descriptor allocator in our engine as the global allocator
	DescriptorAllocatorGrowable globalDescriptorAllocator;

	//We need to store the descriptor set that will bind our render image, and the descriptor layout for that type of descriptor, which we will need later for creating the pipeline
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	//Pipelines and corresponding layouts (Pipelines are created from PipelineLayouts which are the definition of the pipeline for what it as far as descriptors which are just resources)
	//https://vkguide.dev/docs/new_chapter_2/vulkan_shader_code/
	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// dearIMGUI immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	//For rendering a mesh
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	//Image layout for textures
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	GPUSceneData sceneData;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;


	//Default image textures
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	//Image samplers for using texture images
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	//default material we can use for testing as part of the load sequence of the engine
	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	//All loaded meshes
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	//Stats for basic performance checking
	EngineStats stats;

private:

	//Private initializers to setup Vulkan, called by init()
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_background_pipelines();
	void init_imgui();
	void init_mesh_pipeline();
	void init_default_data();

	//Swapchain setup
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void resize_swapchain();


};




