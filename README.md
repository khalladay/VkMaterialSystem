# VkMaterialSystem

This is a project to try out building a generic(ish) material system for Vulkan. Currently it only supports loading a single material (defined in json) onto a full screen quad.

Most of the actual material loading logic and data types (which is the whole point of this project) is found in material_loading.cpp, and asset_rdata_types.h.

Shaders should be written in Vulkan GLSL, and located in data/shaders, running the ShaderPipeline program will create the required files in data/_generated. Materials are defined in data/Materials. 


More information on [my website](http://kylehalladay.com/blog/tutorial/2017/11/27/Vulkan-Material-System.html)
