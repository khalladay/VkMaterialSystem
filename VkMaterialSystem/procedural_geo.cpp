#include "stdafx.h"

#include "procedural_geo.h"
#include "Mesh.h"
#include <vector>

namespace Mesh
{
	uint32_t quad(float width, float height, float xOffset, float yOffset)
	{
		std::vector<Vertex> verts;

		float wComp = width / 2.0f;
		float hComp = height / 2.0f;

		glm::vec3 lbCorner = glm::vec3(-wComp + xOffset, -hComp + yOffset, 0.0f);
		glm::vec3 ltCorner = glm::vec3(lbCorner.x, hComp + yOffset, 0.0f);
		glm::vec3 rbCorner = glm::vec3(wComp + xOffset, lbCorner.y, 0.0f);
		glm::vec3 rtCorner = glm::vec3(rbCorner.x, ltCorner.y, 0.0f);

		verts.push_back({ rtCorner,  glm::vec2(1.0f,1.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ ltCorner, glm::vec2(0.0f,1.0f), glm::vec4(0.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ lbCorner,glm::vec2(0.0f,0.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });
		verts.push_back({ rbCorner, glm::vec2(1.0f,0.0f), glm::vec4(1.0f,1.0f,1.0f,1.0f) });

		uint32_t indices[6] = { 0,2,1,2,0,3 };

		return make(&verts[0], static_cast<uint32_t>(verts.size()), &indices[0], 6);
	}
}