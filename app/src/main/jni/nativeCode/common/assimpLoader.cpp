/*
 *    Copyright 2016 Anand Muralidhar
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "assimpLoader.h"
#include "myShader.h"
#include "misc.h"
#include <opencv2/opencv.hpp>


/**
 * Class constructor, loads shaders & gets locations of variables in them
 */
AssimpLoader::AssimpLoader() {
    importerPtr = new Assimp::Importer;
    scene = NULL;
    isObjectLoaded = false;

    // shader related setup -- loading, attribute and uniform locations
    std::string vertexShader    = "shaders/modelTextured.vsh";
    std::string fragmentShader  = "shaders/modelTextured.fsh";
    shaderProgramID         = LoadShaders(vertexShader, fragmentShader);
    vertexAttribute         = GetAttributeLocation(shaderProgramID, "vertexPosition");
    vertexUVAttribute       = GetAttributeLocation(shaderProgramID, "vertexUV");
    mvpLocation             = GetUniformLocation(shaderProgramID, "mvpMat");
    textureSamplerLocation  = GetUniformLocation(shaderProgramID, "textureSampler");

    CheckGLError("AssimpLoader::AssimpLoader");
}

/**
 * Class destructor, deletes Assimp importer pointer and removes 3D model from GL
 */
AssimpLoader::~AssimpLoader() {
    Delete3DModel();
    if(importerPtr) {
        delete importerPtr;
        importerPtr = NULL;
    }
    scene = NULL; // gets deleted along with importerPtr
}

/**
 * Generate buffers for vertex positions, texture coordinates, faces -- and load data into them
 */
void AssimpLoader::GenerateGLBuffers() {

    struct MeshInfo newMeshInfo; // this struct is updated for each mesh in the model
    GLuint buffer;

    // For every mesh -- load face indices, vertex positions, vertex texture coords
    // also copy texture index for mesh into newMeshInfo.textureIndex

    MyLOGE("scene->mNumMeshes %d=", scene->mNumMeshes);

    for (unsigned int n = 0; n < scene->mNumMeshes; ++n) {

        const aiMesh *mesh = scene->mMeshes[n]; // read the n-th mesh

        // create array with faces
        // convert from Assimp's format to array for GLES
        unsigned int *faceArray = new unsigned int[mesh->mNumFaces * 3];
        unsigned int faceIndex = 0;
        for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {

            // read a face from assimp's mesh and copy it into faceArray
            const aiFace *face = &mesh->mFaces[t];
            memcpy(&faceArray[faceIndex], face->mIndices, 3 * sizeof(unsigned int));
            faceIndex += 3;

        }
        newMeshInfo.numberOfFaces = scene->mMeshes[n]->mNumFaces;

        // buffer for faces
        if (newMeshInfo.numberOfFaces) {

            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(unsigned int) * mesh->mNumFaces * 3, faceArray,
                         GL_STATIC_DRAW);
            newMeshInfo.faceBuffer = buffer;

        }
        delete[] faceArray;

        // buffer for vertex positions
        if (mesh->HasPositions()) {

            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(float) * 3 * mesh->mNumVertices, mesh->mVertices,
                         GL_STATIC_DRAW);
            newMeshInfo.vertexBuffer = buffer;

        }

        // buffer for vertex texture coordinates
        // ***ASSUMPTION*** -- handle only one texture for each mesh
        if (mesh->HasTextureCoords(0)) {

            float * textureCoords = new float[2 * mesh->mNumVertices];
            for (unsigned int k = 0; k < mesh->mNumVertices; ++k) {
                textureCoords[k * 2] = mesh->mTextureCoords[0][k].x;
                textureCoords[k * 2 + 1] = mesh->mTextureCoords[0][k].y;
            }
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(float) * 2 * mesh->mNumVertices, textureCoords,
                         GL_STATIC_DRAW);
            newMeshInfo.textureCoordBuffer = buffer;
            delete[] textureCoords;

        }

        // unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // copy texture index (= texture name in GL) for the mesh from textureNameMap
        aiMaterial *mtl = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturePath;	//contains filename of texture
        if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath)) {
            unsigned int textureId = textureNameMap[texturePath.data];
            newMeshInfo.textureIndex = textureId;
            MyLOGE("newMeshInfo.textureIndex= %d", textureId);
        } else {
            newMeshInfo.textureIndex = 0;
        }

        modelMeshes.push_back(newMeshInfo);
    }
}

/**
 * Read textures associated with all materials and load images to GL
 */
bool AssimpLoader::LoadTexturesToGL(std::string modelFilename) {

    // read names of textures associated with all materials
    textureNameMap.clear();

    for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {

        int textureIndex = 0;
        aiString textureFilename;
        aiReturn isTexturePresent = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE,
                                                                     textureIndex,
                                                                     &textureFilename);

        MyLOGE("LoadTexturesToGL=== %s", textureFilename.data);
        MyLOGE("LoadTexturesToGL=== textureIndex %d", textureIndex);

        while (isTexturePresent == AI_SUCCESS) {
            //fill map with textures, OpenGL image ids set to 0
            textureNameMap.insert(std::pair<std::string, GLuint>(textureFilename.data, 0));

            // more textures? more than one texture could be associated with a material
            textureIndex++;
            isTexturePresent = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE,
                                                                textureIndex, &textureFilename);

            MyLOGE("LoadTexturesToGL %s", textureFilename.C_Str());
            MyLOGE("LoadTexturesToGL textureIndex %d", textureIndex);
        }


//        int textureIndex = 0;
//        aiString textureFilename;
//
//        while (scene->mMaterials[m]->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
//            aiReturn isTexturePresent = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE,
//                                                                         textureIndex,
//                                                                         &textureFilename);
//            if (isTexturePresent == AI_SUCCESS) {
//                //fill map with textures, OpenGL image ids set to 0
//                textureNameMap.insert(std::pair<std::string, GLuint>(textureFilename.data, 0));
//                // more textures? more than one texture could be associated with a material
//                textureIndex++;
//            }
//        }
    }

    int numTextures = (int) textureNameMap.size();
    MyLOGI("Total number of textures is %d ", numTextures);

    // create and fill array with texture names in GL
    GLuint * textureGLNames = new GLuint[numTextures];
    glGenTextures(numTextures, textureGLNames);

    // Extract the directory part from the file name
    // will be used to read the texture
    std::string modelDirectoryName = GetDirectoryName(modelFilename);

    // iterate over the textures, read them using OpenCV, load into GL
    std::map<std::string, GLuint>::iterator textureIterator = textureNameMap.begin();
    int i = 0;
    for (; textureIterator != textureNameMap.end(); ++i, ++textureIterator) {

        std::string textureFilename = (*textureIterator).first;  // get filename
        std::string textureFullPath = modelDirectoryName + "/" + textureFilename;
        (*textureIterator).second = textureGLNames[i];	  // save texture id for filename in map

        // load the texture using OpenCV
        MyLOGI("Loading texture %s", textureFullPath.c_str());
        cv::Mat textureImage = cv::imread(textureFullPath);
        if (!textureImage.empty()) {
            // 加载纹理数据

            // opencv reads textures in BGR format, change to RGB for GL
            cv::cvtColor(textureImage, textureImage, CV_BGR2RGB);
            // opencv reads image from top-left, while GL expects it from bottom-left
            // vertically flip the image
            cv::flip(textureImage, textureImage, 0);

            // bind the texture
            glBindTexture(GL_TEXTURE_2D, textureGLNames[i]);
            // specify linear filtering 指定放大，缩小滤波
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            // load the OpenCV Mat into GLES
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureImage.cols,
                         textureImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         textureImage.data);
            CheckGLError("AssimpLoader::loadGLTexGen");

        } else {

            MyLOGE("Couldn't load texture %s", textureFilename.c_str());

            //Cleanup and return
            delete[] textureGLNames;
            return false;

        }
    }

    //Cleanup and return
    delete[] textureGLNames;
    return true;
}

/**
 * Loads a general OBJ with many meshes -- assumes texture is associated with each mesh
 * does not handle material properties (like diffuse, specular, etc.)
 */
bool AssimpLoader::Load3DModel(std::string modelFilename) {

    MyLOGI("Scene will be imported now");
    scene = importerPtr->ReadFile(modelFilename, aiProcessPreset_TargetRealtime_Quality);

    // Check if import failed
    if (!scene) {
        std::string errorString = importerPtr->GetErrorString();
        MyLOGE("Scene import failed: %s", errorString.c_str());
        return false;
    }
    MyLOGI("Imported %s successfully.", modelFilename.c_str());

    if(!LoadTexturesToGL(modelFilename)) {
        MyLOGE("Unable to load textures");
        return false;
    }
    MyLOGI("Loaded textures successfully");

    GenerateGLBuffers();
    MyLOGI("Loaded vertices and texture coords successfully");

    isObjectLoaded = true;
    return true;
}

/**
 * Clears memory associated with the 3D model
 */
void AssimpLoader::Delete3DModel() {
    if (isObjectLoaded) {
        // clear modelMeshes stuff
//        for (unsigned int i = 0; i < modelMeshes.size(); ++i) {
//            glDeleteTextures(1, &(modelMeshes[i].textureIndex));
//        }
        modelMeshes.clear();

        MyLOGI("Deleted Assimp object");
        isObjectLoaded = false;
    }
}

/**
 * Renders the 3D model by rendering every mesh in the object
 */
void AssimpLoader::Render3DModel(glm::mat4 *mvpMat) {

    if (!isObjectLoaded) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgramID);
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, (const GLfloat *) mvpMat);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(textureSamplerLocation, 0); // 0: 纹理阶段

    unsigned int numberOfLoadedMeshes = modelMeshes.size();
    MyLOGI("numberOfLoadedMeshes is %d ", numberOfLoadedMeshes);

    // render all meshes
    for (unsigned int n = 0; n < numberOfLoadedMeshes; ++n) {

//        MyLOGE("modelMeshes[n].textureIndex= %d", modelMeshes[n].textureIndex);

        // Texture
        if (modelMeshes[n].textureIndex) {
            glBindTexture( GL_TEXTURE_2D, modelMeshes[n].textureIndex);
        }

        // Faces
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelMeshes[n].faceBuffer);

        // Vertices
        glBindBuffer(GL_ARRAY_BUFFER, modelMeshes[n].vertexBuffer);
        glEnableVertexAttribArray(vertexAttribute);
/*
        第一个参数指定我们要配置的顶点属性。还记得我们在顶点着色器中使用layout(location = 0)定义了position顶点属性的位置值(Location)吗？它可以把顶点属性的位置值设置为0。因为我们希望把数据传递到这一个顶点属性中，所以这里我们传入0。
        第二个参数指定顶点属性的大小。顶点属性是一个vec3，它由3个值组成，所以大小是3。
        第三个参数指定数据的类型，这里是GL_FLOAT(GLSL中vec*都是由浮点数值组成的)。
        下个参数定义我们是否希望数据被标准化(Normalize)。如果我们设置为GL_TRUE，所有数据都会被映射到0（对于有符号型signed数据是-1）到1之间。我们把它设置为GL_FALSE。
        第五个参数叫做步长(Stride)，它告诉我们在连续的顶点属性组之间的间隔。由于下个组位置数据在3个GLfloat之后，我们把步长设置为3 * sizeof(GLfloat)。要注意的是由于我们知道这个数组是紧密排列的（在两个顶点属性之间没有空隙）我们也可以设置为0来让OpenGL决定具体步长是多少（只有当数值是紧密排列时才可用）。一旦我们有更多的顶点属性，我们就必须更小心地定义每个顶点属性之间的间隔，我们在后面会看到更多的例子(译注: 这个参数的意思简单说就是从这个属性第二次出现的地方到整个数组0位置之间有多少字节)。
        最后一个参数的类型是GLvoid*，所以需要我们进行这个奇怪的强制类型转换。它表示位置数据在缓冲中起始位置的偏移量(Offset)。由于位置数据在数组的开头，所以这里是0。我们会在后面详细解释这个参数
*/
        glVertexAttribPointer(vertexAttribute, 3, GL_FLOAT, 0, 0, 0);

        // Texture coords
        glBindBuffer(GL_ARRAY_BUFFER, modelMeshes[n].textureCoordBuffer);
        glEnableVertexAttribArray(vertexUVAttribute);
        glVertexAttribPointer(vertexUVAttribute, 2, GL_FLOAT, 0, 0, 0);

        glDrawElements(GL_TRIANGLES, modelMeshes[n].numberOfFaces * 3, GL_UNSIGNED_INT, 0);

        // unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
//        glBindTexture(GL_TEXTURE_2D, 0);
    }

    CheckGLError("AssimpLoader::renderObject() ");

}

