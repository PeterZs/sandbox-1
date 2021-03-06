#include "model-io.hpp"

#include <assert.h>
#include <fstream>

#include "third-party/tinyobj/tiny_obj_loader.h"
#include "third-party/tinyply/tinyply.h"
#include "third-party/meshoptimizer/meshoptimizer.hpp"
#include "fbx-importer.hpp"
#include "model-io-util.hpp"

std::map<std::string, runtime_mesh> import_model(const std::string & path)
{
    std::map<std::string, runtime_mesh> results;

    auto & ext = get_extension(path);

    if (ext == "FBX" || ext == "fbx")
    {
        auto asset = import_fbx_model(path);
        for (auto & a : asset) results[a.first] = a.second;
    }
    else if (ext == "OBJ" || ext == "obj")
    {
        auto asset = import_obj_model(path);
        for (auto & a : asset) results[a.first] = a.second;
    }
    else
    {
        throw std::runtime_error("cannot import model format");
    }

    return results;
}

std::map<std::string, runtime_mesh> import_fbx_model(const std::string & path)
{
#   if (USING_FBX == 1)
    
    std::map<std::string, runtime_mesh> results;

    try
    {
        auto asset = import_fbx_file(path);
        for (auto & a : asset.meshes) results[a.first] = a.second;
        return results;
     }
    catch (const std::exception & e)
    {
        std::cout << "fbx import exception: " << e.what() << std::endl;
    }

    return {};

    #else
        #pragma message ("fbxsdk is not enabled with the SYSTEM_HAS_FBX_SDK flag")
    #endif
    return {};
}

std::map<std::string, runtime_mesh> import_obj_model(const std::string & path)
{
    std::map<std::string, runtime_mesh> meshes;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string parentDir = parent_directory_from_filepath(path) + "/";

    std::string err;
    bool status = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str(), parentDir.c_str());

    if (status && !err.empty())
    {
        std::cout << "tinyobj warning: " << err << std::endl;
    }

    // Append `default` material
    materials.push_back(tinyobj::material_t());

    std::cout << "# of shapes    : " << shapes.size() << std::endl;
    std::cout << "# of materials : " << materials.size() << std::endl;

    // Parse tinyobj data into geometry struct
    for (unsigned int i = 0; i < shapes.size(); i++)
    {
        tinyobj::shape_t * shape = &shapes[i];
        tinyobj::mesh_t * mesh = &shapes[i].mesh;

        runtime_mesh & g = meshes[shape->name];

        std::cout << "Submesh Name:  " << shape->name << std::endl;
        std::cout << "Num Indices:   " << mesh->indices.size() << std::endl;
        std::cout << "Num TexCoords: " << attrib.texcoords.size() << std::endl;

        size_t indexOffset = 0;

        // de-duplicate vertices
        unordered_map_generator<unique_vertex, uint32_t>::Type uniqueVertexMap;

        for (size_t f = 0; f < mesh->num_face_vertices.size(); f++)
        {
            assert(mesh->num_face_vertices[f] == 3);

            uint3 indices;
            for (int v = 0; v < 3; v++)
            {
                const tinyobj::index_t idx = mesh->indices[indexOffset + v];

                unique_vertex vertex;
                vertex.position = { attrib.vertices[3 * idx.vertex_index + 0], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2] };
                vertex.normal = { attrib.normals[3 * idx.normal_index + 0], attrib.normals[3 * idx.normal_index + 1], attrib.normals[3 * idx.normal_index + 2] };
                if (idx.texcoord_index != -1) vertex.texcoord = { attrib.texcoords[2 * idx.texcoord_index + 0], attrib.texcoords[2 * idx.texcoord_index + 1] };

                auto it = uniqueVertexMap.find(vertex);
                if (it != uniqueVertexMap.end())
                {
                    indices[v] = it->second; // found duplicated vertex
                }
                else
                {
                    // we haven't run into this vertex yet
                    uint32_t index = uint32_t(g.vertices.size());

                    uniqueVertexMap[vertex] = index;
                    indices[v] = index;

                    g.vertices.push_back(vertex.position);
                    g.normals.push_back(vertex.normal);
                    g.texcoord0.push_back(vertex.texcoord);
                }
            }

            if (mesh->material_ids[f] > 0) g.material.push_back(mesh->material_ids[f]);
            g.faces.push_back(indices);
            indexOffset += 3;
        }

        /*
        for (int i = 0; i < attrib.colors.size(); i += 3)
        {
            g.colors.push_back({ attrib.colors[i + 0], attrib.colors[i + 1], attrib.colors[i + 2], 1 });
        }
        */
    }

    return meshes;
}

void optimize_model(runtime_mesh & input)
{
    constexpr size_t cacheSize = 32;

    PostTransformCacheStatistics inputStats = analyzePostTransform(&input.faces[0].x, input.faces.size() * 3, input.vertices.size(), cacheSize);
    std::cout << "input acmr: " << inputStats.acmr << ", cache hit %: " << inputStats.hit_percent << std::endl;

    std::vector<uint32_t> inputIndices;
    std::vector<uint32_t> reorderedIndices(input.faces.size() * 3);

    for (auto f : input.faces)
    {
        inputIndices.push_back(f.x);
        inputIndices.push_back(f.y);
        inputIndices.push_back(f.z);
    }

    {
        optimizePostTransform(reorderedIndices.data(), inputIndices.data(), inputIndices.size(), input.vertices.size(), cacheSize);
    }

    std::vector<float3> reorderedVertexBuffer(input.vertices.size());

    {
        optimizePreTransform(reorderedVertexBuffer.data(), input.vertices.data(), reorderedIndices.data(), reorderedIndices.size(), input.vertices.size(), sizeof(float3));
    }

    input.faces.clear();
    for (int i = 0; i < reorderedIndices.size(); i += 3)
    {
        input.faces.push_back(uint3(reorderedIndices[i], reorderedIndices[i + 1], reorderedIndices[i + 2]));
    }

    input.vertices.swap(reorderedVertexBuffer);

    PostTransformCacheStatistics outStats = analyzePostTransform(&input.faces[0].x, input.faces.size() * 3, input.vertices.size(), cacheSize);
    std::cout << "output acmr: " << outStats.acmr << ", cache hit %: " << outStats.hit_percent << std::endl;
}

runtime_mesh import_mesh_binary(const std::string & path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) throw std::runtime_error("couldn't open");

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    runtime_mesh_binary_header h;
    file.read((char*)&h, sizeof(runtime_mesh_binary_header));

    assert(h.headerVersion == runtime_mesh_binary_version);
    if (h.compressionVersion > 0) assert(h.compressionVersion == runtime_mesh_compression_version);

    runtime_mesh mesh;

    mesh.vertices.resize(h.verticesBytes / sizeof(float3));
    mesh.normals.resize(h.normalsBytes / sizeof(float3));
    mesh.colors.resize(h.colorsBytes / sizeof(float3));
    mesh.texcoord0.resize(h.texcoord0Bytes / sizeof(float2));
    mesh.texcoord1.resize(h.texcoord1Bytes / sizeof(float2));
    mesh.tangents.resize(h.tangentsBytes / sizeof(float3));
    mesh.bitangents.resize(h.bitangentsBytes / sizeof(float3));
    mesh.faces.resize(h.facesBytes / sizeof(uint3));
    mesh.material.resize(h.materialsBytes / sizeof(uint32_t));

    file.read((char*)mesh.vertices.data(), h.verticesBytes);
    file.read((char*)mesh.normals.data(), h.normalsBytes);
    file.read((char*)mesh.colors.data(), h.colorsBytes);
    file.read((char*)mesh.texcoord0.data(), h.texcoord0Bytes);
    file.read((char*)mesh.texcoord1.data(), h.texcoord1Bytes);
    file.read((char*)mesh.tangents.data(), h.tangentsBytes);
    file.read((char*)mesh.bitangents.data(), h.bitangentsBytes);
    file.read((char*)mesh.faces.data(), h.facesBytes);
    file.read((char*)mesh.material.data(), h.materialsBytes);

    return mesh;
}

void export_mesh_binary(const std::string & path, runtime_mesh & mesh, bool compressed)
{
    auto file = std::ofstream(path, std::ios::out | std::ios::binary);

    runtime_mesh_binary_header header;
    header.headerVersion = runtime_mesh_binary_version;
    header.compressionVersion = (compressed) ? runtime_mesh_compression_version : 0;
    header.verticesBytes = (uint32_t)mesh.vertices.size() * sizeof(float3);
    header.normalsBytes = (uint32_t)mesh.normals.size() * sizeof(float3);
    header.colorsBytes = (uint32_t)mesh.colors.size() * sizeof(float3);
    header.texcoord0Bytes = (uint32_t)mesh.texcoord0.size() * sizeof(float2);
    header.texcoord1Bytes = (uint32_t)mesh.texcoord1.size() * sizeof(float2);
    header.tangentsBytes = (uint32_t)mesh.tangents.size() * sizeof(float3);
    header.bitangentsBytes = (uint32_t)mesh.bitangents.size() * sizeof(float3);
    header.facesBytes = (uint32_t)mesh.faces.size() * sizeof(uint3);
    header.materialsBytes = (uint32_t)mesh.material.size() * sizeof(uint32_t);

    file.write(reinterpret_cast<char*>(&header), sizeof(runtime_mesh_binary_header));
    file.write(reinterpret_cast<char*>(mesh.vertices.data()), header.verticesBytes);
    file.write(reinterpret_cast<char*>(mesh.normals.data()), header.normalsBytes);
    file.write(reinterpret_cast<char*>(mesh.colors.data()), header.colorsBytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord0.data()), header.texcoord0Bytes);
    file.write(reinterpret_cast<char*>(mesh.texcoord1.data()), header.texcoord1Bytes);
    file.write(reinterpret_cast<char*>(mesh.tangents.data()), header.tangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.bitangents.data()), header.bitangentsBytes);
    file.write(reinterpret_cast<char*>(mesh.faces.data()), header.facesBytes);
    file.write(reinterpret_cast<char*>(mesh.material.data()), header.materialsBytes);

    file.close();
}
