#include "shadow_renderer.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GLES3/gl3.h>

#include <stddef.h>
#include <stdint.h>

namespace {

constexpr char maskVertexShader[] = R"glsl(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_decimals_ed;
layout(location = 2) in vec2 a_base;
layout(location = 3) in vec2 a_height;
uniform mat4 u_matrix;
uniform float u_base;
uniform float u_height;
uniform float u_base_t;
uniform float u_height_t;
uniform bool u_base_attribute;
uniform bool u_height_attribute;
uniform float u_height_factor;

vec2 unpack_float(float packedValue) {
    int encoded = int(packedValue);
    int first = encoded / 256;
    return vec2(first, encoded - first * 256);
}

void main() {
    float base = u_base_attribute ? mix(a_base.x, a_base.y, u_base_t) : u_base;
    float height = u_height_attribute ? mix(a_height.x, a_height.y, u_height_t) : u_height;
    base = max(0.0, base);
    height = max(0.0, height);
    float upper = mod(a_decimals_ed.x, 2.0);
    vec2 decimals = unpack_float(floor(a_decimals_ed.x / 2.0)) / 128.0;
    float tileUnitsPerMeter = -u_height_factor;
    vec2 direction = vec2(-0.5, -0.5) * tileUnitsPerMeter * 0.38;
    vec2 projected = a_pos + decimals + direction * mix(base, height, upper);
    gl_Position = u_matrix * vec4(projected, 0.0, 1.0);
}
)glsl";

constexpr char maskFragmentShader[] = R"glsl(#version 300 es
precision mediump float;
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(1.0); }
)glsl";

constexpr char compositeVertexShader[] = R"glsl(#version 300 es
precision highp float;
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
    v_uv = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)glsl";

constexpr char compositeFragmentShader[] = R"glsl(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_mask;
uniform float u_alpha;
layout(location = 0) out vec4 out_color;
void main() {
    float alpha = texture(u_mask, v_uv).r * u_alpha;
    out_color = vec4(0.0, 0.0, 0.0, alpha);
}
)glsl";

GLuint compileShader(ShadowInstance* instance, GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) return shader;
    char log[1024]{};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    shadowLog(instance, 3, log);
    glDeleteShader(shader);
    return 0;
}

GLuint createProgram(ShadowInstance* instance, const char* vertexSource, const char* fragmentSource) {
    const GLuint vertex = compileShader(instance, GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compileShader(instance, GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_TRUE) return program;
    char log[1024]{};
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    shadowLog(instance, 3, log);
    glDeleteProgram(program);
    return 0;
}

void deleteFramebufferResources(ShadowOpenGLState& state) {
    if (state.depthStencil) glDeleteRenderbuffers(1, &state.depthStencil);
    if (state.maskTexture) glDeleteTextures(1, &state.maskTexture);
    if (state.framebuffer) glDeleteFramebuffers(1, &state.framebuffer);
    state.depthStencil = 0;
    state.maskTexture = 0;
    state.framebuffer = 0;
    state.width = 0;
    state.height = 0;
}

bool ensurePrograms(ShadowInstance* instance) {
    auto& state = instance->gl;
    if (!state.maskProgram) {
        state.maskProgram = createProgram(instance, maskVertexShader, maskFragmentShader);
    }
    if (!state.compositeProgram) {
        state.compositeProgram = createProgram(instance, compositeVertexShader, compositeFragmentShader);
    }
    if (!state.vertexArray) glGenVertexArrays(1, &state.vertexArray);
    if (!state.quadBuffer) {
        constexpr float quad[8]{-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
        glGenBuffers(1, &state.quadBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, state.quadBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    }
    return state.maskProgram && state.compositeProgram && state.vertexArray && state.quadBuffer;
}

bool ensureFramebuffer(ShadowInstance* instance, uint32_t width, uint32_t height) {
    auto& state = instance->gl;
    width = width > 0 ? width : 1u;
    height = height > 0 ? height : 1u;
    if (state.framebuffer && state.width == width && state.height == height) return true;
    deleteFramebufferResources(state);

    glGenTextures(1, &state.maskTexture);
    glBindTexture(GL_TEXTURE_2D, state.maskTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glGenRenderbuffers(1, &state.depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, state.depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    glGenFramebuffers(1, &state.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.maskTexture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, state.depthStencil);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        shadowLog(instance, 3, "fill-extrusion shadow framebuffer is incomplete");
        deleteFramebufferResources(state);
        return false;
    }
    state.width = width;
    state.height = height;
    return true;
}

GLenum attributeElementType(mln_plugin_attribute_type type) {
    switch (type) {
        case MLN_PLUGIN_ATTRIBUTE_INT16_X2:
            return GL_SHORT;
        case MLN_PLUGIN_ATTRIBUTE_UINT16_X2:
            return GL_UNSIGNED_SHORT;
        case MLN_PLUGIN_ATTRIBUTE_FLOAT:
        case MLN_PLUGIN_ATTRIBUTE_FLOAT_X2:
            return GL_FLOAT;
        default:
            return 0;
    }
}

GLint attributeElementCount(mln_plugin_attribute_type type) {
    switch (type) {
        case MLN_PLUGIN_ATTRIBUTE_INT16_X2:
        case MLN_PLUGIN_ATTRIBUTE_UINT16_X2:
        case MLN_PLUGIN_ATTRIBUTE_FLOAT_X2:
            return 2;
        case MLN_PLUGIN_ATTRIBUTE_FLOAT:
            return 1;
        default:
            return 0;
    }
}

bool bindAttribute(GLuint location, const mln_plugin_buffer_binding_v1& binding) {
    const auto type = attributeElementType(binding.type);
    const auto count = attributeElementCount(binding.type);
    if (!binding.buffer || !type || !count) {
        glDisableVertexAttribArray(location);
        glVertexAttrib2f(location, 0.0f, 0.0f);
        return false;
    }
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(binding.buffer));
    glEnableVertexAttribArray(location);
    glVertexAttribPointer(location,
                          count,
                          type,
                          GL_FALSE,
                          static_cast<GLsizei>(binding.stride),
                          reinterpret_cast<const void*>(static_cast<uintptr_t>(binding.offset)));
    return true;
}

void setUniform(GLuint program, const char* name, float value) {
    glUniform1f(glGetUniformLocation(program, name), value);
}

} // namespace

mln_plugin_status shadowOpenGLPrepare(ShadowInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !frame) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    GLint previousFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    const bool success = ensurePrograms(instance) && ensureFramebuffer(instance, frame->width, frame->height);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    return success ? MLN_PLUGIN_STATUS_OK : MLN_PLUGIN_STATUS_CALLBACK_ERROR;
}

mln_plugin_status shadowOpenGLRender(ShadowInstance* instance, const mln_plugin_frame_context_v1* frame) {
    if (!instance || !frame) return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    if (!ensurePrograms(instance) || !ensureFramebuffer(instance, frame->width, frame->height)) {
        return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    }

    GLint targetFramebuffer = 0;
    GLint targetViewport[4]{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &targetFramebuffer);
    glGetIntegerv(GL_VIEWPORT, targetViewport);

    auto& state = instance->gl;
    glBindVertexArray(state.vertexArray);
    glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
    glViewport(0, 0, state.width, state.height);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilMask(0xFF);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    glUseProgram(state.maskProgram);

    for (size_t i = 0; i < frame->fill_extrusion_packet_count; ++i) {
        const auto& packet = frame->fill_extrusion_packets[i];
        if (packet.kind != MLN_PLUGIN_DRAW_PACKET_TRIANGLES || !packet.index_buffer || !packet.position.buffer ||
            !packet.decimals_edge.buffer) {
            continue;
        }
        bindAttribute(0, packet.position);
        bindAttribute(1, packet.decimals_edge);
        const bool hasBase = packet.base_is_attribute && bindAttribute(2, packet.base);
        const bool hasHeight = packet.height_is_attribute && bindAttribute(3, packet.height);
        glUniformMatrix4fv(glGetUniformLocation(state.maskProgram, "u_matrix"), 1, GL_FALSE, packet.tile_matrix);
        setUniform(state.maskProgram, "u_base", packet.constant_base);
        setUniform(state.maskProgram, "u_height", packet.constant_height);
        setUniform(state.maskProgram, "u_base_t", packet.base_interpolation);
        setUniform(state.maskProgram, "u_height_t", packet.height_interpolation);
        setUniform(state.maskProgram, "u_height_factor", packet.height_factor);
        glUniform1i(glGetUniformLocation(state.maskProgram, "u_base_attribute"), hasBase ? 1 : 0);
        glUniform1i(glGetUniformLocation(state.maskProgram, "u_height_attribute"), hasHeight ? 1 : 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(packet.index_buffer));
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(packet.index_count),
                       GL_UNSIGNED_SHORT,
                       reinterpret_cast<const void*>(static_cast<uintptr_t>(packet.index_offset)));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(targetFramebuffer));
    glViewport(targetViewport[0], targetViewport[1], targetViewport[2], targetViewport[3]);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(state.compositeProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.maskTexture);
    glUniform1i(glGetUniformLocation(state.compositeProgram, "u_mask"), 0);

    float opacity = 1.0f;
    if (frame->fill_extrusion_packet_count) {
        opacity = frame->fill_extrusion_packets[0].layer_opacity;
    }
    setUniform(state.compositeProgram, "u_alpha", 0.35f * opacity);
    glBindBuffer(GL_ARRAY_BUFFER, state.quadBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    return glGetError() == GL_NO_ERROR ? MLN_PLUGIN_STATUS_OK : MLN_PLUGIN_STATUS_CALLBACK_ERROR;
}

void shadowOpenGLContextLost(ShadowInstance* instance) {
    if (!instance) return;
    instance->gl = {};
}

void shadowOpenGLDestroy(ShadowInstance* instance) {
    if (!instance) return;
    auto& state = instance->gl;
    deleteFramebufferResources(state);
    if (state.quadBuffer) glDeleteBuffers(1, &state.quadBuffer);
    if (state.vertexArray) glDeleteVertexArrays(1, &state.vertexArray);
    if (state.compositeProgram) glDeleteProgram(state.compositeProgram);
    if (state.maskProgram) glDeleteProgram(state.maskProgram);
    state = {};
}
