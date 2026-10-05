#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <fstream>
#include <GL/glew.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>

#define GLT_IMPLEMENTATION
#include "../gltext.h"

#include "spiders.hpp"

using namespace std;
using namespace glm;

float delta_time = 0;
float last_time = 0.0f;

string load_file(const string &path)
{
    ifstream file(path);
    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
    bool embedded = (argc > 1);
    Window parent = 0;
    Display *x11dpy = NULL;
    int winWidth = 800, winHeight = 600;

    if (embedded)
    {
        parent = (Window)strtoul(argv[1], NULL, 0);
        x11dpy = XOpenDisplay(NULL);
        if (!x11dpy)
        {
            fprintf(stderr, "XOpenDisplay failed\n");
            return -1;
        }
        XWindowAttributes attrs;
        XGetWindowAttributes(x11dpy, parent, &attrs);
        winWidth = attrs.width;
        winHeight = attrs.height;
    }

    if (!glfwInit())
    {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    if (embedded)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    }

    GLFWwindow *window = glfwCreateWindow(winWidth, winHeight, "SUPER", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "glfwCreateWindow failed\n");
        return -1;
    }

    if (embedded)
    {
        Window native = glfwGetX11Window(window);
        XReparentWindow(x11dpy, native, parent, 0, 0);
        XResizeWindow(x11dpy, native, winWidth, winHeight);
        XMapWindow(x11dpy, native);
        XFlush(x11dpy);
    }

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    string vertexShaderSource = load_file("vertex_shader.glsl");
    string fragmentShaderSource = load_file("fragment_shader.glsl");
    if (vertexShaderSource.empty() || fragmentShaderSource.empty())
        return -1;

    const char *vertSrc = vertexShaderSource.c_str();
    const char *fragSrc = fragmentShaderSource.c_str();

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertSrc, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[1000];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 1000, NULL, infoLog);
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragSrc, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 1000, NULL, infoLog);
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 1000, NULL, infoLog);
        printf("Linking Error : %s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if (!gltInit())
    {
        return -1;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    Spider *spider_second = new Spider(80, 0, 0);
    spider_second->get_transform()->yaw = radians(180.0);
    Spider *spider_minute = new Spider(40, 0, 0);
    spider_minute->get_transform()->yaw = radians(180.0);
    Spider *spider_hour = new Spider(0, 0, 0);
    spider_hour->get_transform()->yaw = radians(180.0);

    while (!glfwWindowShouldClose(window))
    {
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        glClearColor(0.133f, 0.141f, 0.212f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        float current_time = glfwGetTime();
        delta_time = current_time - last_time;
        last_time = current_time;

        spider_second->speed = radians(6.0) * 80.0f;
        spider_second->get_transform()->yaw -= radians(6.0) * delta_time;
        spider_second->get_transform()->x += -sinf(spider_second->get_transform()->yaw) * cosf(spider_second->get_transform()->pitch) * spider_second->speed * delta_time;
        spider_second->get_transform()->z += -cosf(spider_second->get_transform()->yaw) * cosf(spider_second->get_transform()->pitch) * spider_second->speed * delta_time;

        spider_minute->speed = radians(0.1) * 40.0f;
        spider_minute->get_transform()->yaw -= radians(0.1) * delta_time;
        spider_minute->get_transform()->x += -sinf(spider_minute->get_transform()->yaw) * cosf(spider_minute->get_transform()->pitch) * spider_minute->speed * delta_time;
        spider_minute->get_transform()->z += -cosf(spider_minute->get_transform()->yaw) * cosf(spider_minute->get_transform()->pitch) * spider_minute->speed * delta_time;

        spider_hour->get_transform()->yaw -= radians(0.00166666666) * delta_time;

        float fov = 45.0f * 3.14159f / 180.0f;
        float aspect = fbHeight > 0 ? (float)fbWidth / (float)fbHeight : 1.0f;
        float near = 0.1f;
        float far = 1000.0f;

        mat4 view = rotate(mat4(1.0f), radians(45.0f), vec3(1.0f, 0.0f, 0.0f));
        view = translate(view, vec3(0.0f, -150.0f, -175.0f));

        mat4 projection = perspective(fov, aspect, near, far);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, value_ptr(projection));

        spider_second->update(delta_time);
        spider_minute->update(delta_time);
        spider_hour->update(delta_time);

        spider_second->draw(modelLoc);
        spider_minute->draw(modelLoc);
        spider_hour->draw(modelLoc);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    gltTerminate();
    glfwTerminate();
    if (embedded)
    {
        XCloseDisplay(x11dpy);
    }
    return 0;
}
