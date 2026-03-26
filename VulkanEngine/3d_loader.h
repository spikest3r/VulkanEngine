#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "vertex.h"
#include "engine_types.h"

class ENGINE_API Mesh : public IResource {
	friend class Model;
	friend class Engine;
private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	VkBuffer verticesVk;
	VkBuffer indicesVk;

	VkDeviceMemory verticesVkMem;
	VkDeviceMemory indicesVkMem;

	Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	Mesh() {};

	void destroy(void*) override;
	ResourceType getType() override;
	PxConvexMesh* convexMesh;
	PxTriangleMesh* triMesh;
};

class ENGINE_API Model {
public:
	Model(const char* path) {
		loadModel(path);
	}
	std::vector<Mesh> meshes;
	void loadModel(const std::string& path);
private:
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::string directory;
};