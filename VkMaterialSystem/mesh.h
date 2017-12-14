#pragma once
#include "stdafx.h"

struct MeshAsset;
struct MeshRenderData;
struct VertexRenderData;

struct Vertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec4 col;
};

namespace Mesh
{
	uint32_t make(Vertex* vertices, uint32_t vertexCount, uint32_t* indices, uint32_t indexCount);

	MeshRenderData getRenderData(uint32_t meshId);
	const VertexRenderData* vertexRenderData();

	void destroy();
}