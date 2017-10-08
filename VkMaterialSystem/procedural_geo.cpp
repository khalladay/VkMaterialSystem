#include "procedural_geo.h"
#include "Mesh.h"
#include <vector>

namespace Mesh
{
	void quad()
	{
		std::vector<Vertex> verts;

		verts.push_back({ glm::vec3(1.0f,1.0f,0.0f),  glm::vec2(1.0f,1.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ glm::vec3(-1.0f,1.0f,0.0f), glm::vec2(0.0f,1.0f), glm::vec4(0.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ glm::vec3(-1.0f,-1.0f,0.0f),glm::vec2(0.0f,0.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ glm::vec3(1.0f,-1.0f,0.0f), glm::vec2(1.0f,0.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });

		uint32_t indices[6] = { 0,2,1,2,0,3 };

		make(&verts[0], static_cast<uint32_t>(verts.size()), &indices[0], 6);
	}
}