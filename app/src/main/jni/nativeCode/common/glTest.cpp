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

#include <iostream>
#include <gl3stub.h>
#include "glTest.h"

GlTest::GlTest() {
    vertices = {
            0.5f, 0.5f, 0.0f,   // 右上角
            0.5f, -0.5f, 0.0f,  // 右下角
            -0.5f, -0.5f, 0.0f, // 左下角
            -0.5f, 0.5f, 0.0f   // 左上角
    };
    // 注意索引从0开始
    indices = {
            0, 1, 3, // 第一个三角形
            1, 2, 3  // 第二个三角形
    };
    vertexShaderSource = "layout (location = 0) in vec3 position;"
            "\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.x, position.y, position.z, 1.0);\n"
            "}";
    fragmentShaderSource = "out vec4 color;\n"
            "\n"
            "void main()\n"
            "{\n"
            "    color = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
            "}";
}

GlTest::~GlTest() {

}

void GlTest::Init() {
    // 创建顶点着色器
    GLuint vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // 检测编译结果
    GLint success;
    GLchar log[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, log);
        std::cout << "ERROR: VERTEX SHADER COMPILE FAILED: " << log << std::endl;
        return;
    }

    // 创建片段（像素）着色器
    GLuint fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // 检测编译结果
    GLint success2;
    GLchar log2[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success2);
    if (!success2) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, log2);
        std::cout << "ERROR: FRAGMENT SHADER COMPILE FAILED: " << log2 << std::endl;
        return;
    }

    // 链接着色器程序
    shaderProgram = glCreateProgram();
    glAttachShader(vertexShader, shaderProgram);
    glAttachShader(fragmentShader, shaderProgram);
    glLinkProgram(shaderProgram);
    // 检测链接结果
    GLint success3;
    GLchar log3[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success3);
    if (!success3) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, log3);
        std::cout << "ERROR: SHADER LINK PROGRAM COMPILE FAILED: " << log3 << std::endl;
        return;
    }

    // 删除着色器,把着色器对象链接到程序对象以后，记得删除着色器对象，不再需要
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 1. 绑定VAO,顶点数组对象可以像顶点缓冲对象那样被绑定，任何随后的顶点属性调用都会储存在这个VAO中
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    // 2. 创建顶点缓冲对象并加载数据
    GLuint VBO;
    glGenBuffers(1, &VBO);
    // 每个顶点属性从一个VBO管理的内存中获得它的数据，而具体是从哪个VBO（程序中可以有多个VBO）
    // 获取则是通过在调用glVetexAttribPointer时绑定到GL_ARRAY_BUFFER的VBO决定的。
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // 3. 创建索引缓冲对象并加载数据
    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    // 4. 设置顶点属性指针
    // 现在，我们已经把输入顶点数据发送给了GPU，并指示了GPU如何在顶点和片段着色器中处理它。
    // 但还没结束，OpenGL还不知道它该如何解释内存中的顶点数据，以及它该如何将顶点数据链接到顶点着色器的属性上。
    // 我们需要告诉OpenGL怎么做。
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0); // 启用顶点属性,顶点属性默认是禁用的
    // 解绑VAO，后面就可以直接使用这个VAO了
    glBindVertexArray(0);
}

void GlTest::Render() {
    // 当我们渲染一个物体时要使用着色器程序
    glUseProgram(shaderProgram);
    // 绑定VAO
    glBindVertexArray(VAO);
    // 绘制物体
//    glDrawArrays(GL_TRIANGLES, 0, 3); // 从顶点缓冲渲染
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); // 从索引缓冲渲染
    // 解绑VAO
    glBindVertexArray(0);
}

