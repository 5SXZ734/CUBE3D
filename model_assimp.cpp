// model_assimp.cpp - Load 3D models using Assimp
#include "model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cstdio>
#include <cstring>

std::string ModelLoader::GetDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) {
        return filepath.substr(0, pos + 1);
    }
    return "";
}

bool ModelLoader::LoadXFile(const char* filepath, Model& outModel) {
    outModel.clear();
    outModel.directory = GetDirectory(filepath);

    Assimp::Importer importer;
    
    // Load with post-processing
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |           // Convert polygons to triangles
        aiProcess_GenNormals |            // Generate normals if missing
        aiProcess_FlipUVs |               // Flip V coordinate (OpenGL convention)
        aiProcess_JoinIdenticalVertices | // Optimize vertex data
        aiProcess_SortByPType);           // Sort by primitive type

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::fprintf(stderr, "Assimp error: %s\n", importer.GetErrorString());
        return false;
    }

    std::printf("Loaded model: %s\n", filepath);
    std::printf("  Meshes: %d\n", scene->mNumMeshes);
    std::printf("  Materials: %d\n", scene->mNumMaterials);

    // Process all meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        
        ModelMesh modelMesh;
        modelMesh.rendererMeshHandle = 0;
        modelMesh.rendererTextureHandle = 0;

        // Extract vertices
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            ModelVertex vertex;
            
            // Position
            vertex.px = mesh->mVertices[v].x;
            vertex.py = mesh->mVertices[v].y;
            vertex.pz = mesh->mVertices[v].z;
            
            // Normal
            if (mesh->HasNormals()) {
                vertex.nx = mesh->mNormals[v].x;
                vertex.ny = mesh->mNormals[v].y;
                vertex.nz = mesh->mNormals[v].z;
            } else {
                vertex.nx = 0.0f;
                vertex.ny = 1.0f;
                vertex.nz = 0.0f;
            }
            
            // Texture coordinates (first set)
            if (mesh->mTextureCoords[0]) {
                vertex.u = mesh->mTextureCoords[0][v].x;
                vertex.v = mesh->mTextureCoords[0][v].y;
            } else {
                vertex.u = 0.0f;
                vertex.v = 0.0f;
            }
            
            modelMesh.vertices.push_back(vertex);
        }

        // Extract indices
        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned int idx = 0; idx < face.mNumIndices; idx++) {
                modelMesh.indices.push_back(face.mIndices[idx]);
            }
        }

        // Extract texture path
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
                
                // Make full path
                std::string fullPath = outModel.directory + texPath.C_Str();
                modelMesh.texturePath = fullPath;
                
                std::printf("  Texture: %s\n", fullPath.c_str());
            }
        }

        std::printf("  Mesh %d: %zu vertices, %zu indices\n", 
                   i, modelMesh.vertices.size(), modelMesh.indices.size());

        outModel.meshes.push_back(modelMesh);
    }

    return true;
}

bool ModelLoader::LoadOBJ(const char* filepath, Model& outModel) {
    // Use Assimp for OBJ too - it's more robust
    return LoadXFile(filepath, outModel);
}
