#include "gltf_layer.hpp"

#include <GLES3/gl3.h>

#include <cstddef>
#include <string>

namespace {

struct OpenGLResources {
    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint vertexArray = 0;
    GLint matrixLocation = -1;
    GLint opacityLocation = -1;
    uint64_t generation = 0;
};

constexpr char vertexSource[] = R"(
#version 300 es
precision highp float;
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
uniform mat4 u_matrix;
out vec3 v_normal;
out vec4 v_color;
void main() {
    gl_Position = u_matrix * vec4(a_position, 1.0);
    v_normal = normalize(vec3(a_normal.x, -a_normal.z, a_normal.y));
    v_color = a_color;
}
)";

constexpr char fragmentSource[] = R"(
#version 300 es
precision mediump float;
in vec3 v_normal;
in vec4 v_color;
uniform float u_opacity;
out vec4 fragColor;
void main() {
    vec3 light = normalize(vec3(-0.45, -0.55, 0.75));
    float diffuse = 0.35 + 0.65 * abs(dot(normalize(v_normal), light));
    float alpha = clamp(v_color.a * u_opacity, 0.0, 1.0);
    fragColor = vec4(v_color.rgb * diffuse * alpha, alpha);
}
)";

GLuint compile(GLenum type, const char* source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) return shader;
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    error.resize(length > 0 ? static_cast<size_t>(length) : 1);
    glGetShaderInfoLog(shader, length, nullptr, error.data());
    glDeleteShader(shader);
    return 0;
}

bool initialize(OpenGLResources& resources, GltfLayerInstance* instance) {
    std::string error;
    resources.vertexShader = compile(GL_VERTEX_SHADER, vertexSource, error);
    resources.fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (!resources.vertexShader || !resources.fragmentShader) {
        gltfLog(instance, 3, "GLTF OpenGL shader compilation failed: " + error);
        return false;
    }
    resources.program = glCreateProgram();
    glAttachShader(resources.program, resources.vertexShader);
    glAttachShader(resources.program, resources.fragmentShader);
    glLinkProgram(resources.program);
    GLint ok = GL_FALSE;
    glGetProgramiv(resources.program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        gltfLog(instance, 3, "GLTF OpenGL program link failed");
        return false;
    }
    resources.matrixLocation = glGetUniformLocation(resources.program, "u_matrix");
    resources.opacityLocation = glGetUniformLocation(resources.program, "u_opacity");
    glGenVertexArrays(1, &resources.vertexArray);
    glGenBuffers(1, &resources.vertexBuffer);
    glGenBuffers(1, &resources.indexBuffer);
    return true;
}

void upload(OpenGLResources& resources, const GltfLayerInstance& instance) {
    glBindVertexArray(resources.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(instance.model->vertices.size() * sizeof(GltfVertex)),
                 instance.model->vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, resources.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(instance.model->indices.size() * sizeof(uint32_t)),
                 instance.model->indices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GltfVertex), reinterpret_cast<void*>(offsetof(GltfVertex, color)));
    resources.generation = instance.modelGeneration;
}

void destroy(OpenGLResources* resources) {
    if (!resources) return;
    if (resources->vertexArray) glDeleteVertexArrays(1, &resources->vertexArray);
    if (resources->vertexBuffer) glDeleteBuffers(1, &resources->vertexBuffer);
    if (resources->indexBuffer) glDeleteBuffers(1, &resources->indexBuffer);
    if (resources->program) glDeleteProgram(resources->program);
    if (resources->vertexShader) glDeleteShader(resources->vertexShader);
    if (resources->fragmentShader) glDeleteShader(resources->fragmentShader);
    delete resources;
}

} // namespace

mln_plugin_status gltfRenderOpenGL(GltfLayerInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !instance->model || !frame) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    auto* resources = static_cast<OpenGLResources*>(instance->openGL);
    if (!resources) {
        resources = new OpenGLResources();
        instance->openGL = resources;
        if (!initialize(*resources, instance)) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }
    if (resources->generation != instance->modelGeneration) upload(*resources, *instance);
    float matrix[16];
    float opacity = 1.0f;
    if (!gltfModelMatrix(frame, matrix, opacity) || opacity <= 0.0f) return MLN_PLUGIN_STATUS_OK;

    glViewport(0, 0, static_cast<GLsizei>(frame->width), static_cast<GLsizei>(frame->height));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(resources->program);
    glUniformMatrix4fv(resources->matrixLocation, 1, GL_FALSE, matrix);
    glUniform1f(resources->opacityLocation, opacity);
    glBindVertexArray(resources->vertexArray);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(instance->model->indices.size()),
                   GL_UNSIGNED_INT,
                   nullptr);
    return glGetError() == GL_NO_ERROR ? MLN_PLUGIN_STATUS_OK : MLN_PLUGIN_STATUS_CALLBACK_ERROR;
}

void gltfDestroyOpenGL(GltfLayerInstance* instance) {
    if (!instance) return;
    destroy(static_cast<OpenGLResources*>(instance->openGL));
    instance->openGL = nullptr;
}
