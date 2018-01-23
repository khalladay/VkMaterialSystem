#include "stdafx.h"

#include "material.h"
#include "asset_rdata_types.h"
#include "hash.h"
#include "vkh.h"
#include "vkh_initializers.h"
#include <vector>
#include "texture.h"
#include <map>
#include "material_creation.h"

struct MaterialStorage
{
	//std::vector<MaterialAsset> data;
	std::map<uint32_t, MaterialAsset> data;
	MaterialAsset mat;
};

MaterialStorage matStorage;

struct GlobalShaderData
{
	__declspec(align(16)) glm::float32 time;
	__declspec(align(16)) glm::vec4 mouse;
	__declspec(align(16)) glm::vec2 resolution;
	__declspec(align(16)) glm::mat4 viewMatrix;
	__declspec(align(16)) glm::vec4 worldSpaceCameraPos;
};

//
namespace Material
{
	//set 0, binding 0 - global uniforms
	vkh::Allocation			globalMem;
	VkBuffer				globalBuffer;
	GlobalShaderData		globalShaderData;
	uint32_t				globalSize;
	void*					globalMappedMemory;

	VkSampler*				globalSamplers;
	VkDescriptorSet*		globalDescSets;
	VkDescriptorSetLayout*	globalDescSetLayouts;
	VkWriteDescriptorSet	globalTextureSetWrite;
	VkDescriptorSetLayoutCreateInfo globalDescSetLayoutCreateInfo;
	VkDescriptorImageInfo*	globalImageInfos;
	VkImageView*			globalImageViews;

	//I want to to move the creation of the global descriptor set to here, out of material creation (it shouldn't be there anyway), to 
	//pave the way for a second global descriptor set (neither should live in an individual material) that contains all the textures we need

	VkDescriptorSet* getGlobalDescSets()
	{
		return globalDescSets;
	}

	void initGlobalShaderData()
	{
		static bool isInitialized = false;
		if (!isInitialized)
		{
			uint32_t structSize = static_cast<uint32_t>(sizeof(GlobalShaderData));
			size_t uboAlignment = vkh::GContext.gpu.deviceProps.limits.minUniformBufferOffsetAlignment;
			size_t s = sizeof(glm::float32);
			size_t s2 = sizeof(glm::vec4);
			size_t s3 = sizeof(glm::mat4);

			float* t = (&globalShaderData.time);
			glm::vec4* mp = (&globalShaderData.mouse);
			glm::vec2* res = (&globalShaderData.resolution);

			globalSize = (structSize / uboAlignment) * uboAlignment + ((structSize % uboAlignment) > 0 ? uboAlignment : 0);
			
			vkh::createBuffer(globalBuffer, 
				globalMem,
				globalSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			vkMapMemory(vkh::GContext.device, globalMem.handle, globalMem.offset, globalSize, 0, &globalMappedMemory);
			isInitialized = true;
			

			globalDescSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * 2);
			globalDescSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * 2);


			globalSamplers = (VkSampler*)malloc(sizeof(VkSampler) * 8);

			VkSamplerCreateInfo createInfo[] =
			{
				vkh::samplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f),
				vkh::samplerCreateInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f),

				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f),
				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_MIPMAP_MODE_LINEAR, 4.0f),
				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_MIPMAP_MODE_LINEAR, 8.0f),

				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f),
				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR, 4.0f),
				vkh::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_MIPMAP_MODE_LINEAR, 8.0f),

			};

			for (uint32_t i = 0; i < 8; ++i)
			{
				VkResult res = vkCreateSampler(vkh::GContext.device, &createInfo[i], 0, &globalSamplers[i]);
				checkf(res == VK_SUCCESS, "Error creating global sampler");
			}

			//globalImageViews = (VkImageView*)malloc(sizeof(VkImageView) * 4096);

			/*for (uint32_t i = 0; i < 4096; ++i)
			{
				globalImageInfos[i].sampler = globalSamplers[2];
				globalImageInfos[i].imageView = globalImageViews[i];
				globalImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		*/

			VkDescriptorSetLayoutBinding globalLayoutBindings[] = 
			{
				vkh::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1),
				vkh::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 8),
	//			vkh::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 2, 4096) //the "num descriptors" is the array size
			};
		
			//when we add the texture array, ,this will have to be updated
			globalDescSetLayoutCreateInfo = vkh::descriptorSetLayoutCreateInfo(globalLayoutBindings, 2);

			VkResult vkres = vkCreateDescriptorSetLayout(vkh::GContext.device, &globalDescSetLayoutCreateInfo, nullptr, &globalDescSetLayouts[0]);
			checkf(vkres == VK_SUCCESS, "Error creating global descriptor set layout");
		
			//no matter how many bindings we have, we'll only ever have 1 global descriptor set
			VkDescriptorSetAllocateInfo allocInfo = vkh::descriptorSetAllocateInfo(&globalDescSetLayouts[0], 1, vkh::GContext.descriptorPool);
			vkres = vkAllocateDescriptorSets(vkh::GContext.device, &allocInfo, &globalDescSets[0]);
			checkf(vkres == VK_SUCCESS, "Error allocating global descriptor set");

			VkWriteDescriptorSet globalWrites[2];

			VkDescriptorImageInfo samplerInfo[8];
			for (uint32_t i = 0; i < 8; ++i) samplerInfo[i].sampler = globalSamplers[i];

			//set 0 binding 0 -> global uniform memory 
			{
				VkDescriptorBufferInfo globalBufferInfo = {};
				globalBufferInfo.offset = 0;
				globalBufferInfo.buffer = globalBuffer;
				globalBufferInfo.range = globalSize;

				globalWrites[0] = {};
				globalWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				globalWrites[0].dstSet = globalDescSets[0];
				globalWrites[0].dstBinding = 0; //refers to binding in shader
				globalWrites[0].dstArrayElement = 0;
				globalWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				globalWrites[0].descriptorCount = 1;
				globalWrites[0].pBufferInfo = &globalBufferInfo;
				globalWrites[0].pImageInfo = 0;
				globalWrites[0].pTexelBufferView = nullptr; // Optional
			}
			//set 0 binding 1 -> global sampler array
			{

				globalWrites[1] = {};
				globalWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				globalWrites[1].dstBinding = 1;
				globalWrites[1].dstArrayElement = 0;
				globalWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				globalWrites[1].descriptorCount = 8;
				globalWrites[1].dstSet = globalDescSets[0];
				globalWrites[1].pBufferInfo = 0;
				globalWrites[1].pImageInfo = samplerInfo;
			}
			////set 0 binding 2 -> global texture array
			//{
			//	globalWrites[2] = {};
			//	globalWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			//	globalWrites[2].dstBinding = 2;
			//	globalWrites[2].dstArrayElement = 0;
			//	globalWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			//	globalWrites[2].descriptorCount = 4096;
			//	globalWrites[2].pBufferInfo = 0;
			//	globalWrites[2].pImageInfo = 0;
			//}

			vkUpdateDescriptorSets(vkh::GContext.device, 2, globalWrites, 0, nullptr);

		}
	}

	uint32_t charArrayToMaterialName(const char* name)
	{
		return hash(name);
	}

	MaterialInstance make(const char* materialPath)
	{
		uint32_t newId = reserve(materialPath);
		return make(newId, load(materialPath));
	}

	MaterialInstance makeInstance(uint32_t parentId)
	{
		InstanceDefinition def = {};
		def.parent = parentId;
		return makeInstance(def);
	}

	uint32_t reserve(const char* reserveName)
	{
		uint32_t hashedName = charArrayToMaterialName(reserveName);
		
		//if there is any collision, explode because we can't handle that - and maybe should switch to 64 bit hashing? 
		while (matStorage.data.count(hashedName) > 0)
		{
			checkf(0, "hash collision with material name");
		}

		matStorage.data[hashedName] = {};
		return hashedName;
	}

	void setPushConstantData(uint32_t matId, const char* var, void* data, uint32_t size)
	{
		MaterialRenderData& rData = Material::getRenderData(matId);

		uint32_t varHash = hash(var);

		for (uint32_t i = 0; i < rData.pushConstantLayout.memberCount * 2; i += 2)
		{
			if (rData.pushConstantLayout.layout[i] == varHash)
			{
				uint32_t offset = rData.pushConstantLayout.layout[i + 1];
				memcpy(rData.pushConstantData + rData.pushConstantLayout.layout[i + 1], data, size);
				break;
			}
		}
	}

	//note: this cannot be done from within a command buffer
	/*void setTexture(MaterialInstance inst, const char* var, uint32_t texId)
	{
		MaterialRenderData& rData = Material::getRenderData(inst.parent);

		uint32_t varHash = hash(var);
		for (uint32_t i = 0; i < rData.numDynamicUniforms * MATERIAL_UNIFORM_LAYOUT_STRIDE; i += MATERIAL_UNIFORM_LAYOUT_STRIDE)
		{
			if (rData.dynamicUniformLayout[i] == varHash)
			{
				TextureRenderData* texData = Texture::getRenderData(texId);
				uint32_t index = rData.dynamicUniformLayout[i + TEXTUREVIEWPTR_INDEX_IDX];
				uint32_t setWriteIdx = rData.dynamicUniformLayout[i + DESCSET_WRITE_IDX];

				VkWriteDescriptorSet& setWrite = rData.instPages[inst.page].descSetWrites[setWriteIdx * inst.index]; 
				VkDescriptorImageInfo imageInfo = {};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = texData->view; 
				imageInfo.sampler = texData->sampler;

				setWrite.pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(vkh::GContext.device, 1, &setWrite, 0, nullptr);
			}
		}
	}*/
	
	void setUniformData(MaterialInstance inst, const char* name, void* data)
	{
		MaterialRenderData& rData = Material::getRenderData(inst.parent);
		
		uint32_t varHash = hash(name);

		for (uint32_t i = 0; i < rData.numDynamicUniforms * MATERIAL_UNIFORM_LAYOUT_STRIDE; i += MATERIAL_UNIFORM_LAYOUT_STRIDE)
		{
			if (rData.dynamicUniformLayout[i] == varHash)
			{
				VkBuffer& targetBuffer = rData.instPages[inst.page].dynamicBuffer;
				uint32_t size = rData.dynamicUniformLayout[i + MEMBER_SIZE_IDX];
				uint32_t offset = rData.dynamicUniformLayout[i + MEMBER_OFFSET_IDX] + rData.dynamicUniformMemSize * inst.index;

				vkh::VkhCommandBuffer scratch = vkh::beginScratchCommandBuffer(vkh::ECommandPoolType::Transfer);
				vkCmdUpdateBuffer(scratch.buffer, targetBuffer, offset, size, data);
				vkh::submitScratchCommandBuffer(scratch);
				break;
			}
		}
	}

	void setPushConstantVector(uint32_t matId, const char* var, glm::vec4& data)
	{
		setPushConstantData(matId, var, &data, sizeof(glm::vec4));
	}

	void setPushConstantMatrix(uint32_t matId, const char* var, glm::mat4& data)
	{
		setPushConstantData(matId, var, &data, sizeof(glm::mat4));
	}

	void setPushConstantFloat(uint32_t matId, const char* var, float data)
	{
		setPushConstantData(matId, var, &data, sizeof(float));
	}

	void setUniformVector4(MaterialInstance inst, const char* name, glm::vec4& data)
	{
		setUniformData(inst, name, &data);
	}

	void setUniformVector2(MaterialInstance inst, const char* name, glm::vec2& data)
	{
		setUniformData(inst, name, &data);
	}

	void setUniformFloat(MaterialInstance inst, const char* name, float data)
	{
		setUniformData(inst, name, &data);
	}

	void setUniformMatrix(MaterialInstance inst, const char* name, glm::mat4& data)
	{
		setUniformData(inst, name, &data);
	}

	void setGlobalFloat(const char* name, float data)
	{
		initGlobalShaderData();
		globalShaderData.time = data;
		memcpy(globalMappedMemory, &globalShaderData, globalSize);
	}

	void setGlobalVector4(const char* name, glm::vec4& data)
	{
		initGlobalShaderData();
		globalShaderData.mouse = data;
		memcpy(globalMappedMemory, &globalShaderData, globalSize);
	}

	void setGlobalVector2(const char* name, glm::vec2& data)
	{
		initGlobalShaderData();
		globalShaderData.resolution = data;
		memcpy(globalMappedMemory, &globalShaderData, globalSize);
	}


	MaterialRenderData& getRenderData(uint32_t matId)
	{
		return *matStorage.data[matId].rData;
	}

	void destroy()
	{
		assert(0); //unimeplemented
	}

	MaterialAsset& getMaterialAsset(uint32_t matId)
	{
		return matStorage.data[matId];
	}

}