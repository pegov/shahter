#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <iostream>

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
using namespace glm;

#include <ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "shader.h"

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

vec3 camera_pos(0.0f, 0.0f, 3.0f);
vec3 camera_front(0.0f, 0.0f, -1.0f);
vec3 camera_up(0.0f, 1.0f, 0.0f);

vec3 light_pos = vec3(0.0f, 1.5f, -0.5f);

double delta_time = 0.0f;
double last_frame = 0.0f;

float last_x = floor(WINDOW_WIDTH / 2);
float last_y = floor(WINDOW_HEIGHT / 2);

float mouse_yaw = -90.0f;
float mouse_pitch = 0.0f;

struct Fov {
    float normal;
    float temp;
    bool is_close;
};

Fov fov = {
    .normal = 85.0f,
    .temp = 85.0f,
    .is_close = false
};

bool mouse_first = true;

void process_input(GLFWwindow *window) {
    if (
        glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
        || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS
    ) {
        glfwSetWindowShouldClose(window, true);
    }

    float camera_speed = 3.0f * (float)delta_time;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera_speed *= 2.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera_pos += camera_speed * camera_front;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera_pos -= camera_speed * camera_front;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera_pos -= normalize(cross(camera_front, camera_up)) * camera_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera_pos += normalize(cross(camera_front, camera_up)) * camera_speed;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera_pos += camera_speed * camera_up;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        camera_pos -= camera_speed * camera_up;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        if (!fov.is_close) {
            fov.is_close = true;
            fov.temp = fov.normal;
            fov.normal = fov.normal - 30.0f;
            if (fov.normal < 1.0f) {
                fov.normal = 1.0f;
            }
        }
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE) {
        if (fov.is_close) {
            fov.is_close = false;
            fov.normal = fov.temp;
        }
    }
}

bool debug_mode = false;
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        if (debug_mode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        debug_mode = !debug_mode;
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (mouse_first) {
        last_x = xpos;
        last_y = ypos;
        mouse_first = false;
    }

    float xoffset = xpos - last_x;
    float yoffset = last_y - ypos;
    last_x = xpos;
    last_y = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    mouse_yaw += xoffset;
    mouse_pitch += yoffset;

    if (mouse_pitch > 89.0f) {
        mouse_pitch = 89.0f;
    }
    if (mouse_pitch < -89.0f) {
        mouse_pitch = -89.0f;
    }
    vec3 direction;
    direction.x = cos(radians(mouse_yaw)) * cos(radians(mouse_pitch));
    direction.y = sin(radians(mouse_pitch));
    direction.z = sin(radians(mouse_yaw)) * cos(radians(mouse_pitch));
    camera_front = normalize(direction);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    fov.normal -= (float)yoffset;
    if (fov.normal < 1.0f) {
        fov.normal = 1.0f;
    }
    if (fov.normal > 85.0f) {
        fov.normal = 85.0f;
    }
}

void drop_callback(GLFWwindow *window, int count, const char **paths) {
    printf("Drop event\n");
    for (int i = 0; i < count; i++) {
        printf("path %d: %s\n", i, paths[i]);
    }
}

struct Character {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Offset to advance to next glyph
};

std::map<char, Character> Characters;

void render_text(Shader &s, GLuint vao, GLuint vbo, std::string text, float x, float y, float scale, glm::vec3 color)
{
    // activate corresponding render state
    s.use();
    s.setVec3("textColor", color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++)
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

struct BlockCoord {
    float front_x1;
    float front_y1;
    float front_x2;
    float front_y2;

    float back_x1;
    float back_y1;
    float back_x2;
    float back_y2;

    float top_x1;
    float top_y1;
    float top_x2;
    float top_y2;

    float bottom_x1;
    float bottom_y1;
    float bottom_x2;
    float bottom_y2;

    float left_x1;
    float left_y1;
    float left_x2;
    float left_y2;

    float right_x1;
    float right_y1;
    float right_x2;
    float right_y2;
};

inline void set_texture_coords(
    float *vertices,
    BlockCoord c
) {
    vertices[0 * 5 + 3] = c.back_x2;
    vertices[0 * 5 + 4] = c.back_y2;
    vertices[1 * 5 + 3] = c.back_x1;
    vertices[1 * 5 + 4] = c.back_y2;
    vertices[2 * 5 + 3] = c.back_x1;
    vertices[2 * 5 + 4] = c.back_y1;
    vertices[3 * 5 + 3] = c.back_x1;
    vertices[3 * 5 + 4] = c.back_y1;
    vertices[4 * 5 + 3] = c.back_x2;
    vertices[4 * 5 + 4] = c.back_y1;
    vertices[5 * 5 + 3] = c.back_x2;
    vertices[5 * 5 + 4] = c.back_y2;

    vertices[6 * 5 + 3] = c.front_x1;
    vertices[6 * 5 + 4] = c.front_y2;
    vertices[7 * 5 + 3] = c.front_x2;
    vertices[7 * 5 + 4] = c.front_y2;
    vertices[8 * 5 + 3] = c.front_x2;
    vertices[8 * 5 + 4] = c.front_y1;
    vertices[9 * 5 + 3] = c.front_x2;
    vertices[9 * 5 + 4] = c.front_y1;
    vertices[10 * 5 + 3] = c.front_x1;
    vertices[10 * 5 + 4] = c.front_y1;
    vertices[11 * 5 + 3] = c.front_x1;
    vertices[11 * 5 + 4] = c.front_y2;

    vertices[12 * 5 + 3] = c.left_x2;
    vertices[12 * 5 + 4] = c.left_y1;
    vertices[13 * 5 + 3] = c.left_x1;
    vertices[13 * 5 + 4] = c.left_y1;
    vertices[14 * 5 + 3] = c.left_x1;
    vertices[14 * 5 + 4] = c.left_y2;
    vertices[15 * 5 + 3] = c.left_x1;
    vertices[15 * 5 + 4] = c.left_y2;
    vertices[16 * 5 + 3] = c.left_x2;
    vertices[16 * 5 + 4] = c.left_y2;
    vertices[17 * 5 + 3] = c.left_x2;
    vertices[17 * 5 + 4] = c.left_y1;

    vertices[18 * 5 + 3] = c.right_x1;
    vertices[18 * 5 + 4] = c.right_y1;
    vertices[19 * 5 + 3] = c.right_x2;
    vertices[19 * 5 + 4] = c.right_y1;
    vertices[20 * 5 + 3] = c.right_x2;
    vertices[20 * 5 + 4] = c.right_y2;
    vertices[21 * 5 + 3] = c.right_x2;
    vertices[21 * 5 + 4] = c.right_y2;
    vertices[22 * 5 + 3] = c.right_x1;
    vertices[22 * 5 + 4] = c.right_y2;
    vertices[23 * 5 + 3] = c.right_x1;
    vertices[23 * 5 + 4] = c.right_y1;

    vertices[24 * 5 + 3] = c.bottom_x1;
    vertices[24 * 5 + 4] = c.bottom_y2;
    vertices[25 * 5 + 3] = c.bottom_x2;
    vertices[25 * 5 + 4] = c.bottom_y2;
    vertices[26 * 5 + 3] = c.bottom_x2;
    vertices[26 * 5 + 4] = c.bottom_y1;
    vertices[27 * 5 + 3] = c.bottom_x2;
    vertices[27 * 5 + 4] = c.bottom_y1;
    vertices[28 * 5 + 3] = c.bottom_x1;
    vertices[28 * 5 + 4] = c.bottom_y1;
    vertices[29 * 5 + 3] = c.bottom_x1;
    vertices[29 * 5 + 4] = c.bottom_y2;

    vertices[30 * 5 + 3] = c.top_x1;
    vertices[30 * 5 + 4] = c.top_y1;
    vertices[31 * 5 + 3] = c.top_x2;
    vertices[31 * 5 + 4] = c.top_y1;
    vertices[32 * 5 + 3] = c.top_x2;
    vertices[32 * 5 + 4] = c.top_y2;
    vertices[33 * 5 + 3] = c.top_x2;
    vertices[33 * 5 + 4] = c.top_y2;
    vertices[34 * 5 + 3] = c.top_x1;
    vertices[34 * 5 + 4] = c.top_y2;
    vertices[35 * 5 + 3] = c.top_x1;
    vertices[35 * 5 + 4] = c.top_y1;
}

int main() {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "ERROR::FREETYPE: Could not init FreeType Library\n");
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "./resources/FiraCode-Regular.ttf", 0, &face))
    {
        fprintf(stderr, "ERROR::FREETYPE: Failed to load font\n");
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 36);

    glewExperimental = true;
    glfwInit();
    // glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_FLOATING, GL_TRUE);

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

#ifdef __linux__
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "Shahter");
#endif

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Shahter", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to create initialize GLEW\n");
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetDropCallback(window, drop_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    const unsigned char *version = glGetString(GL_VERSION);
    printf("GL VERSION: %s\n", (char *)version);

    int num_extensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    printf("Number of extensions: %d\n", num_extensions);

    Shader block_shader = compile_shader(
        "./shaders/block.vert",
        "./shaders/block.frag"
    );

    Shader skybox_shader = compile_shader(
        "./shaders/skybox.vert",
        "./shaders/skybox.frag"
    );

    Shader font_shader = compile_shader(
        "./shaders/font.vert",
        "./shaders/font.frag"
    );

    float cube_vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    GLuint cube_vao, cube_vbo;
    glGenVertexArrays(1, &cube_vao);
    glGenBuffers(1, &cube_vbo);

    glBindVertexArray(cube_vao);

    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    const char *cubemap_faces[6] = {
        "./resources/skybox/right.jpg",
        "./resources/skybox/left.jpg",
        "./resources/skybox/top.jpg",
        "./resources/skybox/bottom.jpg",
        "./resources/skybox/front.jpg",
        "./resources/skybox/back.jpg"
    };

    float skybox_vertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f
    };

    GLuint skybox_vao, skybox_vbo;
    glGenVertexArrays(1, &skybox_vao);
    glGenBuffers(1, &skybox_vbo);

    glBindVertexArray(skybox_vao);

    glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices), skybox_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    GLuint cubemap_texture_id;
    glGenTextures(1, &cubemap_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_id);

    int i_width, i_height, i_nr_channels;
    for (int i = 0; i < 6; i++) {
        unsigned char *data = stbi_load(cubemap_faces[i], &i_width, &i_height, &i_nr_channels, 0);
        if (data) {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                GL_RGB,
                i_width,
                i_height,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                data
            );
            stbi_image_free(data);
        } else {
            fprintf(stderr, "Failed to load image: %s\n", cubemap_faces[i]);
            stbi_image_free(data);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    float block_vertices[] = {
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,

        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, 0.0f, 0.0f,
    };
    GLuint minecraft_atlas_id;
    glGenTextures(1, &minecraft_atlas_id);
    glBindTexture(GL_TEXTURE_2D, minecraft_atlas_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int atlas_w, atlas_h, nr_channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load("resources/minecraft1.17.png", &atlas_w, &atlas_h, &nr_channels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_w, atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        // glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        fprintf(stderr, "Failed to load minecraft1.17.png\n");
        exit(1);
    }
    stbi_image_free(data);
    block_shader.use();
    block_shader.setInt("texture1", 0);

    int w = atlas_w;
    int h = atlas_h;

    BlockCoord coord = {
        .front_x1 = 384.0f / w,
        .front_y1 = 1.0f - (48.0f / h),
        .front_x2 = 400.0f / w,
        .front_y2 = 1.0f - (64.0f / h),

        .back_x1 = 384.0f / w,
        .back_y1 = 1.0f - (80.0f / h),
        .back_x2 = 400.0f / w,
        .back_y2 = 1.0f - (96.0f / h),

        .top_x1 = 384.0f / w,
        .top_y1 = 1.0f - (96.0f / h),
        .top_x2 = 400.0f / w,
        .top_y2 = 1.0f - (112.0f / h),

        .bottom_x1 = 384.0f / w,
        .bottom_y1 = 1.0f - (96.0f / h),
        .bottom_x2 = 400.0f / w,
        .bottom_y2 = 1.0f - (112.0f / h),

        .left_x1 = 384.0f / w,
        .left_y1 = 1.0f - (80.0f / h),
        .left_x2 = 400.0f / w,
        .left_y2 = 1.0f - (96.0f / h),

        .right_x1 = 384.0f / w,
        .right_y1 = 1.0f - (80.0f / h),
        .right_x2 = 400.0f / w,
        .right_y2 = 1.0f - (96.0f / h),
    };
    set_texture_coords(block_vertices, coord);

    GLuint block_vao, block_vbo;
    glGenVertexArrays(1, &block_vao);
    glGenBuffers(1, &block_vbo);

    glBindVertexArray(block_vao);

    glBindBuffer(GL_ARRAY_BUFFER, block_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(block_vertices), block_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    stbi_set_flip_vertically_on_load(false);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
    for (unsigned char c = 0; c < 128; c++)
    {
        // load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            fprintf(stderr, "ERROR::FREETYPE: Failed to load Glyph");
            continue;
        }
        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );
        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glm::mat4 text_projection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);

    unsigned int text_vao, text_vbo;
    glGenVertexArrays(1, &text_vao);
    glGenBuffers(1, &text_vbo);
    glBindVertexArray(text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    double last_frame_time = glfwGetTime();
    int frames_num = 0;
    int fps = 0;
    double ms = 0.0;

    while (!glfwWindowShouldClose(window)) {

        double current_frame_time = glfwGetTime();
        frames_num++;
        if (current_frame_time - last_frame_time >= 1.0) {
            ms = 1000.0 / double(frames_num);
            fps = frames_num;
            frames_num = 0;
            last_frame_time += 1.0;
        }

        double current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        // input
        process_input(window);

        mat4 view = lookAt(camera_pos, camera_pos + camera_front, camera_up);
        mat4 projection = perspective(radians(fov.normal), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 100.0f);
        mat4 block_model(1.0f);
        block_model = scale(block_model, vec3(0.5f, 0.5f, 0.5f));

        // render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthMask(GL_FALSE);
        skybox_shader.use();
        mat4 skybox_view = mat4(mat3(view));
        skybox_shader.setMat4("view", skybox_view);
        skybox_shader.setMat4("projection", projection);
        glBindVertexArray(skybox_vao);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture_id);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);

        glBindVertexArray(block_vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, minecraft_atlas_id);

        block_shader.use();
        block_shader.setMat4("model", block_model);
        block_shader.setMat4("view", view);
        block_shader.setMat4("projection", projection);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        block_model = translate(block_model, vec3(1.0f, 0.0f, 0.0f));
        block_shader.setMat4("model", block_model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        block_model = translate(block_model, vec3(0.0f, 1.0f, 0.0f));
        block_shader.setMat4("model", block_model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindTexture(GL_TEXTURE_2D, 0);

        font_shader.use();
        font_shader.setMat4("projection", text_projection);
        char fps_text[32];
        char ms_text[32];
        sprintf(fps_text, "fps: %d", fps);
        sprintf(ms_text, "ms: %.2f", ms);
        render_text(font_shader, text_vao, text_vbo, "Shahter v0.0.1", 25.0f, 25.0f, 1.0f, glm::vec3(0.3f, 0.3f, 0.8f));
        render_text(font_shader, text_vao, text_vbo, fps_text, WINDOW_WIDTH - 250.0f, WINDOW_HEIGHT - 70.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
        render_text(font_shader, text_vao, text_vbo, ms_text, WINDOW_WIDTH - 250.0f, WINDOW_HEIGHT - 70.0f - 36.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));

        // clean up
        glBindVertexArray(0);
        glUseProgram(0);

        // poll and swap buffers
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}
