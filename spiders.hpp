#pragma once
#include <stdio.h>
#include <vector>
#include "../glm/glm/glm.hpp"
#include "../glm/glm/gtc/type_ptr.hpp"

using namespace glm;

struct Mesh
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct Transform
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
    mat4 to_matrix() const;
};

class Model
{
    public:
        Model(const Mesh& mesh);
        ~Model();
        void draw() const;

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;

    private:
        unsigned int VAO, VBO, EBO;
        int indexCount;
};

struct LegSeg
{
    Transform transform;
    float length;
};

struct Leg
{
    LegSeg coxa, femur, tibia, tarsus;

    vec3 rest_position;
    vec3 current_target;
    vec3 step_start;
    vec3 step_end;
    float step_offset;
    float step_t = 1.0f, step_cooldown = 0.0f;
};

struct LegAttach
{
    vec3 offset;
    float base_yaw;
};

class Spider
{
    private:
        static Mesh create_body_mesh();
        static Mesh create_legseg_mesh();
        static std::array<LegAttach, 8> create_legattach();

        Model body_model, legseg_model;
        Transform transform;
        std::array<Leg, 8> legs;

        void solve_ik(std::vector<vec3> &positions,
                      const std::vector<float> &lengths,
                      const vec3 &target,
                      int iterations = 10);
        void solve_leg_ik(Leg &leg, const vec3 &worldTarget, float dt);
        void set_seg_angles(Transform &t, const mat3 &parentWorldRot, const vec3 &fromPos, const vec3 &toPos, float dt);
        void update_leg(float dt);

        float lastGroupAStepTime = 0.0f;
        float animTime = 0.0f;

    public:
        Spider(float x, float y, float z);
        ~Spider();
        void draw(unsigned int modelLoc) const;
        void update(float dt);
        Transform* get_transform();
        float speed = 0.0f;
};