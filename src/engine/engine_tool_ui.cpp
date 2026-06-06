#include "engine.h"
#include "engine_tool_ui.h"

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

    // gamepad nav support
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

void Engine::SetUICallback(std::function<void(Engine* engine)> callback) { 
    uiCallback = callback; 
}

void ToolUI::Begin(const char* name)
{
    ImGui::Begin(name);
}

void ToolUI::End()
{
    ImGui::End();
}

bool ToolUI::Button(const char* text)
{
    return ImGui::Button(text);
}

bool ToolUI::Button(const char* text, Vector2 size)
{
    return ImGui::Button(text, ImVec2(size.x,size.y));
}

void ToolUI::Text(const char* text)
{
    ImGui::Text(text);
}

void ToolUI::SameLine()
{
    ImGui::SameLine();
}

void ToolUI::ProgressBar(float value, Vector2 size)
{
    ImGui::ProgressBar(value, ImVec2(size.x, size.y));
}

bool ToolUI::TextField(const char* label, char* buffer, size_t size, bool disallowBlank)
{
    return ImGui::InputText(label, buffer, size, disallowBlank ? ImGuiInputTextFlags_CharsNoBlank : 0);
}

bool ToolUI::InputFloat3(const char* label, Vector3& v, float speed)
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

void ToolUI::SetNextWindowPos(Vector2 pos) {
    //TODO: expose flags
    ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y), ImGuiCond_Always);
}

void ToolUI::SetNextWindowSize(Vector2 size) {
    ImGui::SetNextWindowSize(ImVec2(size.x, size.y), ImGuiCond_Always);
}

void ToolUI::AddFontFromFileTTF(UIFont& font, const char* fontName, float size) {
    ImGuiIO& io = ImGui::GetIO();
    font.font = io.Fonts->AddFontFromFileTTF(fontName, size);
    io.Fonts->Build();
}

void ToolUI::PushFont(UIFont& font) {
    ImGui::PushFont(font.font);
}

void ToolUI::PopFont() {
    ImGui::PopFont();
}

// === DEBUG ===

ImVec2 WorldToScreen(const physx::PxVec3& worldPos, const glm::mat4& viewProj, float width, float height) {
    glm::vec4 clipSpace = viewProj * glm::vec4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    if (clipSpace.w <= 0.0f) return ImVec2(-1, -1);

    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
    return ImVec2(
        (ndc.x + 1.0f) * 0.5f * width,
        (1.0f - ndc.y) * 0.5f * height
    );
}

physx::PxVec3 Vec3ToPx(Vector3& v) {
    return physx::PxVec3(v.x, v.y, v.z);
}

void Engine::renderPhysXDebug(const glm::mat4& viewProjMatrix, float screenWidth, float screenHeight) {
    const PxRenderBuffer& rb = gScene->getRenderBuffer();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    for (PxU32 i = 0; i < rb.getNbLines(); i++) {
        const PxDebugLine& line = rb.getLines()[i];

        ImVec2 p0 = WorldToScreen(line.pos0, viewProjMatrix, screenWidth, screenHeight);
        ImVec2 p1 = WorldToScreen(line.pos1, viewProjMatrix, screenWidth, screenHeight);

        if (p0.x != -1 && p1.x != -1) {
            drawList->AddLine(p0, p1, IM_COL32(0, 255, 0, 255), 1.0f);
        }
    }

    for (auto& r : gRayDebugs)
    {
        ImVec2 p0 = WorldToScreen(Vec3ToPx(r.origin), viewProjMatrix, screenWidth, screenHeight);
        ImVec2 p1 = WorldToScreen(Vec3ToPx(r.hitOrEnd), viewProjMatrix, screenWidth, screenHeight);

        if (p0.x == -1 || p1.x == -1) continue;

        ImU32 color = r.hit ? IM_COL32(0, 0, 255, 255)
            : IM_COL32(255, 0, 0, 255);

        drawList->AddLine(p0, p1, color, 2.0f);

        if (r.hit)
            drawList->AddCircleFilled(p1, 4.0f, color);
    }
}