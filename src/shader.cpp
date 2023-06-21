#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>

#include "shader.h"

static char* get_shader_content(const char *path) {
    FILE *f;
    long size = 0;
    char *buffer;

    f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);

    f = fopen(path, "r");
    buffer = (char *)malloc((size_t)(size + 1));
    fread(buffer, 1, (size_t)size, f);
    buffer[size] = '\0';
    fclose(f);

    return buffer;
}

Shader compile_shader(const char *vertex_shader_path, const char *fragment_shader_path) {
    char *buffer;

    int success;
    char info_log[512];

    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    buffer = get_shader_content(vertex_shader_path);
    if (buffer) {
        glShaderSource(vertex_shader_id, 1, &buffer, NULL);
        glCompileShader(vertex_shader_id);

        glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex_shader_id, 512, NULL, info_log);
            fprintf(
                stderr,
                "ERROR: Failed to compile vertex shader at path %s: %s\n",
                vertex_shader_path,
                info_log
            );
        } else {
            printf("Compiled vertex shader: %s\n", vertex_shader_path);
        }
        free(buffer);
    } else {
        fprintf(
            stderr,
            "ERROR: Failed to open file at path %s\n",
            vertex_shader_path
        );
    }

    buffer = get_shader_content(fragment_shader_path);
    if (buffer) {
        glShaderSource(fragment_shader_id, 1, &buffer, NULL);
        glCompileShader(fragment_shader_id);

        glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment_shader_id, 512, NULL, info_log);
            fprintf(
                stderr,
                "ERROR: Failed to compile fragment shader at path %s: %s\n",
                fragment_shader_path,
                info_log
            );
        } else {
            printf(
                "Compiled fragment shader: %s\n",
                fragment_shader_path
            );
        }
        free(buffer);
    } else {
        fprintf(
            stderr,
            "ERROR: Failed to open file at path %s\n",
            fragment_shader_path
        );
    }

    GLuint program_id = glCreateProgram();

    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);
    glLinkProgram(program_id);

    glGetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program_id, 512, NULL, info_log);
        fprintf(stderr, "ERROR: Failed to link shader program: %s\n", info_log);

        glDeleteProgram(program_id);
    }

    glDetachShader(program_id, vertex_shader_id);
    glDetachShader(program_id, fragment_shader_id);

    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    return Shader {program_id};
}
