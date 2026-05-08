#include "ui.h"

#include "engine.h"

inline void CreateUIQuad(
    std::vector<Vertex>& outVertices,
    std::vector<uint32_t>& outIndices
)
{
    outVertices = {
        // pos                          normal              uv
        {{-0.5f, -0.5f, 0.0f, 1.0f},   {0,0,1},           {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f, 1.0f},   {0,0,1},           {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f, 1.0f},   {0,0,1},           {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f, 1.0f},   {0,0,1},           {0.0f, 1.0f}},
    };

    outIndices = {
        0, 1, 2,
        2, 3, 0
    };
}

void Engine::initUILayer() {
    std::vector<Vertex> UIQuad_v;
    std::vector<uint32_t> UIQuad_i;
    CreateUIQuad(UIQuad_v, UIQuad_i);
    Mesh* mesh = createMesh("engine_quad_ui", UIQuad_v, UIQuad_i);
    mesh->engineMember = true; // prevent destroy until exit
    uiQuad = mesh;
}

UIElement* Engine::createUIElement(Texture* texture, Vector2 pos, Vector2 size) {
    UIElement* element = new UIElement();
    
    element->texture = texture;
    element->position = pos;
    element->size = size;
    
    element->buffer.vertexBuffer = uiQuad->verticesVk;
    element->buffer.indexBuffer = uiQuad->indicesVk;

    element->buffer.vertexBufferMemory = uiQuad->verticesVkMem;
    element->buffer.indexBufferMemory = uiQuad->indicesVkMem;

    createVkDescriptorSet(element->descSet, texture->imageView);
    
    uiElements.push_back(element);

    for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++)
        updateUniformBuffer(f);

    return element;
}