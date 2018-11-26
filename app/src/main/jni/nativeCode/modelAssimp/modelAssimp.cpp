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

#include "myShader.h"
#include "modelAssimp.h"


#include "assimp/Importer.hpp"
#include <opencv2/opencv.hpp>
#include <myJNIHelper.h>

using namespace std;

/**
 * Class constructor
 */
ModelAssimp::ModelAssimp() {

    MyLOGD("ModelAssimp::ModelAssimp");
    initsDone = false;

    // create MyGLCamera object and set default position for the object
    myGLCamera = new MyGLCamera();
    float pos[] = {0., 0., 0., 0.2, 0.5, 0.};
    std::copy(&pos[0], &pos[5], std::back_inserter(modelDefaultPosition));
    myGLCamera->SetModelPosition(modelDefaultPosition);

    modelObject = NULL;
}

ModelAssimp::~ModelAssimp() {

    MyLOGD("ModelAssimp::ModelAssimpssimp");
    if (myGLCamera) {
        delete myGLCamera;
    }
    if (modelObject) {
        delete modelObject;
    }
}

/**
 * Perform inits and load the triangle's vertices/colors to GLES
 */
void ModelAssimp::PerformGLInits() {
    MyLOGD("ModelAssimp::PerformGLInits");

    MyGLInits();
    modelObject = new AssimpLoader();

    CheckGLError("ModelAssimp::PerformGLInits");
    initsDone = true;
}

vector<string> split(const string &s, const string &seperator) {
    vector<string> result;
    typedef string::size_type string_size;
    string_size i = 0;

    while (i != s.size()) {
        //找到字符串中首个不等于分隔符的字母；
        int flag = 0;
        while (i != s.size() && flag == 0) {
            flag = 1;
            for (string_size x = 0; x < seperator.size(); ++x)
                if (s[i] == seperator[x]) {
                    ++i;
                    flag = 0;
                    break;
                }
        }

        //找到又一个分隔符，将两个分隔符之间的字符串取出；
        flag = 0;
        string_size j = i;
        while (j != s.size() && flag == 0) {
            for (string_size x = 0; x < seperator.size(); ++x)
                if (s[j] == seperator[x]) {
                    flag = 1;
                    break;
                }
            if (flag == 0)
                ++j;
        }
        if (i != j) {
            result.push_back(s.substr(i, j - i));
            i = j;
        }
    }
    return result;
}

void ModelAssimp::ResetModel(JNIEnv *env,
                             jobject instance,
                             jstring objFileName,
                             jstring mtlFileName,
                             jstring texFileName) {

    if (initsDone) {
        modelObject->Delete3DModel();
        myGLCamera->Reset(45, 10, 1.0f, 2000.0f);

        std::string newObjFileNameStr;
        const char *cObjFileName = env->GetStringUTFChars(objFileName, NULL);
        std::string objFileNameStr = std::string(cObjFileName);
        bool isFilesPresent = gHelperObject->ExtractAssetReturnFilename(objFileNameStr, newObjFileNameStr);
        MyLOGE("objFileName %s", objFileNameStr.c_str());

        if (!isFilesPresent) {
            MyLOGE("Model %s does not exist!", newObjFileNameStr.c_str());
            return;
        }

        std::string newMtlFileNameStr;
        const char *cMtlFileName = env->GetStringUTFChars(mtlFileName, NULL);
        std::string mtlFileNameStr = std::string(cMtlFileName);
        gHelperObject->ExtractAssetReturnFilename(mtlFileNameStr, newMtlFileNameStr);
        MyLOGE("mtlFileName %s", mtlFileNameStr.c_str());

        const char *cTexFileName = env->GetStringUTFChars(texFileName, NULL);
        std::string texFileNameStr = std::string(cTexFileName);
        std::vector<string> texFileNameArray = split(texFileNameStr, "&");

        for(vector<string>::size_type i = 0; i < texFileNameArray.size(); ++i) {
            std::string newTexFileNameStr;
            gHelperObject->ExtractAssetReturnFilename(texFileNameArray[i], newTexFileNameStr);
            MyLOGE("texFileName %s", texFileNameArray[i].c_str());
        }

        modelObject->Load3DModel(newObjFileNameStr);
    }
}


/**
 * Render to the display
 */
void ModelAssimp::Render() {

    // clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 mvpMat = myGLCamera->GetMVP();
    modelObject->Render3DModel(&mvpMat);

    CheckGLError("ModelAssimp::Render");

}

/**
 * set the viewport, function is also called when user changes device orientation
 */
void ModelAssimp::SetViewport(int width, int height) {

    screenHeight = height;
    screenWidth = width;
    glViewport(0, 0, width, height);
    CheckGLError("Cube::SetViewport");

    myGLCamera->SetAspectRatio((float) width / height);
}


/**
 * reset model's position in double-tap
 */
void ModelAssimp::DoubleTapAction() {

    myGLCamera->SetModelPosition(modelDefaultPosition);
}

/**
 * rotate the model if user scrolls with one finger
 */
void
ModelAssimp::ScrollAction(float distanceX, float distanceY, float positionX, float positionY) {

    myGLCamera->RotateModel(distanceX, distanceY, positionX, positionY);
}

/**
 * pinch-zoom: move the model closer or farther away
 */
void ModelAssimp::ScaleAction(float scaleFactor) {

    myGLCamera->ScaleModel(scaleFactor);
}

/**
 * two-finger drag: displace the model by changing its x-y coordinates
 */
void ModelAssimp::MoveAction(float distanceX, float distanceY) {

    myGLCamera->TranslateModel(distanceX, distanceY);
}