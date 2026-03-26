#include "engine.h"
#include "engine_ui.h"

void Engine::InitImGui(
    GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue graphicsQueue,
    VkRenderPass renderPass,
    uint32_t imageCount
)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * 6;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3; // Or your specific version
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.Queue = graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = imageCount;
    init_info.ImageCount = imageCount;
    
    // NEW: Set the pipeline info in the nested struct
    init_info.PipelineInfoMain.RenderPass = renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
}

void Engine::SetUICallback(std::function<void()> callback) { 
    uiCallback = callback; 
}

void UI::Begin(const char* name)
{
    ImGui::Begin(name);
}

void UI::End()
{
    ImGui::End();
}

bool UI::Button(const char* text)
{
    return ImGui::Button(text);
}

void UI::Text(const char* text)
{
    ImGui::Text(text);
}

void UI::SameLine()
{
    ImGui::SameLine();
}

void UI::ProgressBar(float value, Vector2 size)
{
    ImGui::ProgressBar(value, ImVec2(size.x, size.y));
}

bool UI::TextField(const char* label, char* buffer, size_t size, bool disallowBlank)
{
    return ImGui::InputText(label, buffer, size, disallowBlank ? ImGuiInputTextFlags_CharsNoBlank : 0);
}

bool UI::InputFloat3(const char* label, Vector3& v, float speed)
{
    float arr[3] = { v.x, v.y, v.z };

    bool changed = ImGui::DragFloat3(label, arr, speed);

    if (changed)
    {
        v.x = arr[0];
        v.y = arr[1];
        v.z = arr[2];
    }

    return changed;
}