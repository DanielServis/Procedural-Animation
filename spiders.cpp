#include "spiders.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>

Model::Model(const Mesh& mesh): indexCount(mesh.indices.size())
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

Model::~Model()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Model::draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Spider::Spider(float x, float y, float z) : body_model(create_body_mesh()), legseg_model(create_legseg_mesh())
{
    transform.x = x;
    transform.y = y;
    transform.z = z;
    transform.pitch = 0;
    transform.yaw = 0;
    transform.roll = 0;

    auto attachments = create_legattach();

    const float leg_scale[8] = {
        0.75f, 1.0f, 
        1.0f, 1.25f, 
        0.75f, 1.0f, 
        1.0f, 1.25f
    };

    for (int i = 0; i < 8; ++i)
    {
        Leg &leg = legs[i];
        const LegAttach &att = attachments[i];

        vec3 outwardDir = normalize(vec3(att.offset.x, 0.0f, att.offset.z));
        float aligned_yaw = atan2f(outwardDir.x, -outwardDir.z);

        leg.coxa.transform.x = att.offset.x;
        leg.coxa.transform.y = att.offset.y;
        leg.coxa.transform.z = att.offset.z;
        leg.coxa.transform.yaw = aligned_yaw;
        leg.coxa.transform.pitch = radians(20.0f);
        leg.coxa.length = 6.0f * leg_scale[i];

        leg.femur.transform.x = 0.0f;
        leg.femur.transform.y = -leg.coxa.length; 
        leg.femur.transform.z = 0.0f;
        leg.femur.transform.pitch = radians(40.0f);
        leg.femur.length = 6.0f * leg_scale[i];

        leg.tibia.transform.y = -leg.femur.length;
        leg.tibia.transform.pitch = radians(-70.0f);
        leg.tibia.length = 8.0f * leg_scale[i];

        leg.tarsus.transform.y = -leg.tibia.length;
        leg.tarsus.length = 1.5f * leg_scale[i];

        float legReach = leg.coxa.length + leg.femur.length + leg.tibia.length + leg.tarsus.length; // ≈11.2
        float horizDist = legReach * 0.6f;
        float dropDist = legReach * 0.4f;

        leg.rest_position = vec3(att.offset.x, att.offset.y, att.offset.z) + outwardDir * horizDist + vec3(0.0f, -dropDist, 0.0f);

        vec3 worldRest = vec3(transform.to_matrix() * vec4(leg.rest_position, 1.0f));
        leg.current_target = worldRest;
        leg.step_t = 1.0f;
        leg.step_offset = (i % 2 == 0) ? 0.0f : 0.15f;
        leg.step_start = worldRest;
        leg.step_end = worldRest;
    }
}

Spider::~Spider() = default;

void Spider::draw(unsigned int modelLoc) const
{
    mat4 body_mat = transform.to_matrix();
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(body_mat));
    body_model.draw();

    for (const auto &leg : legs)
    {
        mat4 coxa_mat = body_mat * leg.coxa.transform.to_matrix();
        mat4 femur_mat = coxa_mat * leg.femur.transform.to_matrix();
        mat4 tibia_mat = femur_mat * leg.tibia.transform.to_matrix();
        mat4 tarsus_mat = tibia_mat * leg.tarsus.transform.to_matrix();

        struct SegDraw
        {
            const mat4 *mat;
            float length;
        };
        SegDraw segs[4] = {
            {&coxa_mat, leg.coxa.length},
            {&femur_mat, leg.femur.length},
            {&tibia_mat, leg.tibia.length},
            {&tarsus_mat, leg.tarsus.length},
        };

        for (const auto &seg : segs)
        {
            mat4 finalMat = scale(*seg.mat, vec3(1.0f, seg.length, 1.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(finalMat));
            legseg_model.draw();
        }
    }
}

Transform *Spider::get_transform()
{
    return &transform;
}

mat4 Transform::to_matrix() const
{
    float Cy = cosf(yaw),   Sy = sinf(yaw);
    float Cp = cosf(pitch), Sp = sinf(pitch);
    float Cr = cosf(roll),  Sr = sinf(roll);

    mat4 mat(1.0f);

    mat[0][0] = Cy * Cr + Sy * Sp * Sr;
    mat[0][1] = Cp * Sr;
    mat[0][2] = -Sy * Cr + Cy * Sp * Sr;
    mat[0][3] = 0;

    mat[1][0] = -Cy * Sr + Sy * Sp * Cr;
    mat[1][1] = Cp * Cr;
    mat[1][2] = Sy * Sr + Cy * Sp * Cr;
    mat[1][3] = 0;

    mat[2][0] = Sy * Cp;
    mat[2][1] = -Sp;
    mat[2][2] = Cy * Cp;
    mat[2][3] = 0;

    mat[3][0] = x;
    mat[3][1] = y;
    mat[3][2] = z;
    mat[3][3] = 1;

    return mat;
}

Mesh Spider::create_body_mesh() 
{
    Mesh mesh;

    mesh.vertices = {
        -0.1875f, -0.2025f, -1.25f,  // front square
        0.1875f, -0.2025f, -1.25f,
        -0.1875f, -0.6975f, -1.25f,
        0.1875f, -0.6975f, -1.25f,
        -0.1875f, -0.2025f,  0.25f,  // back square
        0.1875f, -0.2025f,  0.25f,
        -0.1875f, -0.6975f,  0.25f,
        0.1875f, -0.6975f,  0.25f,
        -0.375f,  -0.2025f, -0.875f, // left square
        -0.375f,  -0.2025f, -0.125f,
        -0.375f,  -0.6975f, -0.875f,
        -0.375f,  -0.6975f, -0.125f,
        0.375f,  -0.2025f, -0.875f, // right square
        0.375f,  -0.2025f, -0.125f,
        0.375f,  -0.6975f, -0.875f,
        0.375f,  -0.6975f, -0.125f,
        -0.1875f,  0.045f,  -0.875f, // top square
        0.1875f,  0.045f,  -0.875f,
        -0.1875f,  0.045f,  -0.125f,
        0.1875f,  0.045f,  -0.125f,
        -0.1875f, -0.945f,  -0.875f, // bottom square
        0.1875f, -0.945f,  -0.875f,
        -0.1875f, -0.945f,  -0.125f,
        0.1875f, -0.945f,  -0.125f,

        -0.25f,  0.25f, -0.3125f,  // front square
        0.25f,  0.25f, -0.3125f,
        -0.25f, -0.25f, -0.3125f,
        0.25f, -0.25f, -0.3125f,
        -0.125f, 0.125f, 2.1875f,  // back square
        0.125f, 0.125f, 2.1875f,
        -0.125f,-0.125f, 2.1875f,
        0.125f,-0.125f, 2.1875f,
        -0.5f,   0.25f,  0.3125f,  // left square
        -0.4f,   0.2f,   1.5625f,
        -0.5f,  -0.25f,  0.3125f,
        -0.5f,  -0.25f,  1.5625f,
        0.5f,   0.25f,  0.3125f,  // right square
        0.4f,   0.2f,   1.5625f,
        0.5f,  -0.25f,  0.3125f,
        0.5f,  -0.25f,  1.5625f,
        -0.25f,  0.5f,   0.3125f,  // top square
        0.25f,  0.5f,   0.3125f,
        -0.2f,   0.4f,   1.5625f,
        0.2f,   0.4f,   1.5625f,
        -0.25f, -0.5f,   0.3125f,  // bottom square
        0.25f, -0.5f,   0.3125f,
        -0.25f, -0.5f,   1.5625f,
        0.25f, -0.5f,   1.5625f,
    };

    mesh.indices = {
        0, 1, 2, // squares
        2, 3, 1,
        4, 5, 6,
        6, 7, 5,
        8, 9, 10,
        10, 11, 9,
        12, 13, 14,
        14, 15, 13,
        16, 17, 18,
        18, 19, 17,
        20, 21, 22,
        22, 23, 21,
        0, 2, 8, // connecting squares
        2, 8, 10,
        1, 3, 12,
        3, 12, 14,
        9, 11, 6,
        4, 6, 9,
        13, 15, 5,
        15, 5, 7,
        16, 17, 0,
        0, 1, 17,
        18, 19, 4,
        19, 4, 5,
        16, 18, 8,
        18, 8, 9,
        17, 19, 12,
        19, 12, 13,
        20, 21, 2,
        2, 3, 21,
        22, 23, 6,
        6, 7, 23,
        20, 22, 10,
        10, 11, 22,
        21, 23, 14,
        14, 15, 23,
        0, 8, 16, // triangles front
        1, 12, 17,
        2, 10, 20,
        3, 14, 21,
        4, 9, 18, // triangles back
        5, 13, 19,
        6, 11, 22,
        7, 15, 23,

        24, 25, 26, // squares
        26, 27, 25,
        28, 29, 30,
        30, 31, 29,
        32, 33, 34,
        34, 35, 33,
        36, 37, 38,
        38, 39, 37,
        40, 41, 42,
        42, 43, 41,
        44, 45, 46,
        46, 47, 45,
        24, 26, 32, // connecting squares
        26, 32, 34,
        25, 27, 36,
        27, 36, 38,
        33, 35, 30,
        28, 30, 33,
        37, 39, 29,
        39, 29, 31,
        40, 41, 24,
        24, 25, 41,
        42, 43, 28,
        43, 28, 29,
        40, 42, 32,
        42, 32, 33,
        41, 43, 36,
        43, 36, 37,
        44, 45, 26,
        26, 27, 45,
        46, 47, 30,
        30, 31, 47,
        44, 46, 34,
        34, 35, 46,
        45, 47, 38,
        38, 39, 47,
        24, 32, 40, // triangles front
        25, 36, 41,
        26, 34, 44,
        27, 38, 45,
        28, 33, 42, // triangles back
        29, 37, 43,
        30, 35, 46,
        31, 39, 47
    };

    return mesh;
}

Mesh Spider::create_legseg_mesh()
{
    Mesh mesh;

    mesh.vertices = {
        -0.025f, -0.25f, -0.05f, // front square
        0.025f, -0.25f, -0.05f,
        -0.025f, -0.75f, -0.05f,
        0.025f, -0.75f, -0.05f,
        -0.025f, -0.25f, 0.05f, // back square
        0.025f, -0.25f, 0.05f,
        -0.025f, -0.75f, 0.05f,
        0.025f, -0.75f, 0.05f,
        -0.05f, -0.25f, -0.025f, // left square
        -0.05f, -0.25f, 0.025f,
        -0.05f, -0.75f, -0.025f,
        -0.05f, -0.75f, 0.025f,
        0.05f, -0.25f, -0.025f, // right square
        0.05f, -0.25f, 0.025f,
        0.05f, -0.75f, -0.025f,
        0.05f, -0.75f, 0.025f,
        -0.025f, 0.0f, -0.025f, // top square
        0.025f, 0.0f, -0.025f,
        -0.025f, 0.0f, 0.025f,
        0.025f, 0.0f, 0.025f,
        -0.025f, -1.0f, -0.025f, // bottom square
        0.025f, -1.0f, -0.025f,
        -0.025f, -1.0f, 0.025f,
        0.025f, -1.0f, 0.025f
    };

    mesh.indices = {
        0, 1, 2, // squares
        2, 3, 1,
        4, 5, 6,
        6, 7, 5,
        8, 9, 10,
        10, 11, 9,
        12, 13, 14,
        14, 15, 13,
        16, 17, 18,
        18, 19, 17,
        20, 21, 22,
        22, 23, 21,
        0, 2, 8, // connecting squares
        2, 8, 10,
        1, 3, 12,
        3, 12, 14,
        9, 11, 6,
        4, 6, 9,
        13, 15, 5,
        15, 5, 7,
        16, 17, 0,
        0, 1, 17,
        18, 19, 4,
        19, 4, 5,
        16, 18, 8,
        18, 8, 9,
        17, 19, 12,
        19, 12, 13,
        20, 21, 2,
        2, 3, 21,
        22, 23, 6,
        6, 7, 23,
        20, 22, 10,
        10, 11, 22,
        21, 23, 14,
        14, 15, 23,
        0, 8, 16, // triangles front
        1, 12, 17,
        2, 10, 20,
        3, 14, 21,
        4, 9, 18, // triangles back
        5, 13, 19,
        6, 11, 22,
        7, 15, 23
    };

    return mesh;
}

std::array<LegAttach, 8> Spider::create_legattach()
{
    return {{
        {{0.25f, -0.5f, 0.3f}, radians(-60.0f)},
        {{0.25f, -0.5f, 0.0f}, radians(-30.0f)},
        {{0.25f, -0.5f, -0.2f}, radians(30.0f)},
        {{0.25f, -0.5f, -0.75f}, radians(60.0f)},
        {{-0.25f, -0.5f, 0.3f}, radians(-120.0f)},
        {{-0.25f, -0.5f, 0.0f}, radians(-150.0f)},
        {{-0.25f, -0.5f, -0.2f}, radians(150.0f)},
        {{-0.25f, -0.5f, -0.75f}, radians(120.0f)},
    }};
}

void Spider::solve_ik(std::vector<vec3> &positions,
                 const std::vector<float> &lengths,
                 const vec3 &target,
                 int iterations)
{
    vec3 root = positions[0];
    float totalLength = 0;
    for (float l : lengths)
        totalLength += l;

    if (length(target - root) > totalLength)
    {
        vec3 dir = normalize(target - root);
        for (size_t i = 1; i < positions.size(); ++i)
            positions[i] = positions[i - 1] + dir * lengths[i - 1];
        return;
    }

    for (int iter = 0; iter < iterations; ++iter)
    {
        positions.back() = target;
        for (int i = (int)positions.size() - 2; i >= 0; --i)
        {
            vec3 diff = positions[i] - positions[i + 1];
            if (length(diff) < 1e-5f)
                continue;
            vec3 dir = normalize(diff);
            positions[i] = positions[i + 1] + dir * lengths[i];
        }
        positions[0] = root;
        for (size_t i = 1; i < positions.size(); ++i)
        {
            vec3 diff = positions[i] - positions[i - 1];
            if (length(diff) < 1e-5f)
                continue;
            vec3 dir = normalize(diff);
            positions[i] = positions[i - 1] + dir * lengths[i - 1];
        }
    }
}

void apply_dir_to_transform(Transform &t, const vec3 &localDir)
{
    vec3 dir = normalize(localDir);
    t.yaw = atan2f(-dir.x, -dir.z);
    t.pitch = atan2f(sqrtf(dir.x * dir.x + dir.z * dir.z), -dir.y);
    t.roll = 0.0f;                                   
}

static vec3 clamp_dir_pitch(const vec3 &dir, const vec3 &coxa_horizontal_fallback, float min_pitch, float max_pitch)
{
    vec3 d = normalize(dir);
    float horiz = sqrtf(d.x * d.x + d.z * d.z);
    float pitch = atan2f(-d.y, horiz);
    pitch = clamp(pitch, min_pitch, max_pitch);

    vec3 h = (horiz > 1e-4f) ? vec3(d.x, 0, d.z) / horiz : vec3(coxa_horizontal_fallback);
    return vec3(h.x * cosf(pitch), -sinf(pitch), h.z * cosf(pitch));
}

void Spider::set_seg_angles(Transform &t, const mat3 &parentWorldRot,
                            const vec3 &fromPos, const vec3 &toPos, float dt)
{
    vec3 diff = toPos - fromPos;
    if (length(diff) < 1e-5f)
        return;

    vec3 worldDir = normalize(diff);
    vec3 localDir = inverse(parentWorldRot) * worldDir;

    float horiz = sqrtf(localDir.x * localDir.x + localDir.z * localDir.z);
    const float DEADZONE = 0.1f;

    const float SMOOTH_RATE = 15.0f; 
    float smoothing = 1.0f - expf(-SMOOTH_RATE * dt);

    if (horiz > DEADZONE)
    {
        float targetYaw = atan2f(-localDir.x, -localDir.z);

        float diffAngle = targetYaw - t.yaw;
        while (diffAngle > pi<float>())
            diffAngle -= 2.0f * pi<float>();
        while (diffAngle < -pi<float>())
            diffAngle += 2.0f * pi<float>();

        t.yaw += diffAngle * smoothing;
    }

    float targetPitch = atan2f(horiz, -localDir.y);
    t.pitch += (targetPitch - t.pitch) * smoothing;

    t.roll = 0.0f;
}

void Spider::solve_leg_ik(Leg &leg, const vec3 &worldTarget, float dt)
{
    mat4 invBody = inverse(transform.to_matrix());
    vec3 localTarget = vec3(invBody * vec4(worldTarget, 1.0f));

    vec3 hipPos(leg.coxa.transform.x, leg.coxa.transform.y, leg.coxa.transform.z);

    float L0 = leg.coxa.length;
    float L1 = leg.femur.length;
    float L2 = leg.tibia.length;
    float L3 = leg.tarsus.length;

    std::vector<vec3> positions(5);
    positions[0] = hipPos;

    vec3 toTargetFull = localTarget - hipPos;
    const float coxaPitch = radians(-80.0f); 
    float coxaYaw = atan2f(toTargetFull.x, -toTargetFull.z);

    vec3 coxaDir;
    coxaDir.x = sinf(coxaYaw) * cosf(coxaPitch);
    coxaDir.z = -cosf(coxaYaw) * cosf(coxaPitch);
    coxaDir.y = -sinf(coxaPitch);

    positions[1] = hipPos + coxaDir * L0;

    vec3 kneeBase = positions[1];
    vec3 toTarget = localTarget - kneeBase;
    float distToTarget = length(toTarget);

    float Leff = L2 + L3;
    float maxReach = L1 + Leff;
    float minReach = fabsf(L1 - Leff);
    distToTarget = clamp(distToTarget, minReach + 0.01f, maxReach - 0.01f);

    vec3 toTargetDir = (length(toTarget) > 1e-4f)
                                ? normalize(toTarget)
                                : vec3(0.0f, -1.0f, 0.0f);

    float cosKnee = (L1 * L1 + distToTarget * distToTarget - Leff * Leff) / (2.0f * L1 * distToTarget);
    cosKnee = clamp(cosKnee, -1.0f, 1.0f);
    float kneeAngle = acosf(cosKnee);

    vec3 coxaHorizontal = normalize(vec3(coxaDir.x, 0.0f, coxaDir.z));
    vec3 down(0.0f, -1.0f, 0.0f);
    vec3 up(0.0f, 1.0f, 0.0f);
    vec3 bendAxis = cross(coxaHorizontal, up);
    float bendAxisLen = length(bendAxis);
    if (bendAxisLen < 1e-3f) 
    {
        bendAxis = (fabsf(coxaHorizontal.x) < fabsf(coxaHorizontal.z)) ? vec3(1, 0, 0) : vec3(0, 0, 1);
    }
    else
    {
        bendAxis /= bendAxisLen;
    }

    mat4 rot = rotate(mat4(1.0f), kneeAngle, bendAxis);
    vec3 femurDir = vec3(rot * vec4(toTargetDir, 0.0f));
    femurDir = clamp_dir_pitch(femurDir, coxaHorizontal, radians(5.0f), radians(45.0f));
    positions[2] = kneeBase + femurDir * L1;

    vec3 kneeToTarget = localTarget - positions[2];
    float kneeToTargetDist = length(kneeToTarget);
    vec3 kneeToTargetDir = (kneeToTargetDist > 1e-4f)
                                    ? kneeToTarget / kneeToTargetDist
                                    : vec3(0.0f, -1.0f, 0.0f);

    positions[3] = positions[2] + kneeToTargetDir * L2;
    positions[4] = positions[3] + kneeToTargetDir * L3;

    mat3 identity(1.0f);
    set_seg_angles(leg.coxa.transform, identity, positions[0], positions[1], dt);

    mat3 coxaRot = mat3(leg.coxa.transform.to_matrix());
    set_seg_angles(leg.femur.transform, coxaRot, positions[1], positions[2], dt);

    mat3 femurRot = coxaRot * mat3(leg.femur.transform.to_matrix());
    set_seg_angles(leg.tibia.transform, femurRot, positions[2], positions[3], dt);

    mat3 tibiaRot = femurRot * mat3(leg.tibia.transform.to_matrix());
    set_seg_angles(leg.tarsus.transform, tibiaRot, positions[3], positions[4], dt);
}

void Spider::update(float delta_time)
{
    const float STEP_TRIGGER_DIST = 10.0f;
    const float STEP_SPEED = 3.0f * (speed + 1);
    const float STEP_HEIGHT = 5.0f;
    const float GROUP_STAGGER = 0.1f * (10 / speed);

    animTime += delta_time;

    for (int i = 0; i < 8; ++i)
    {
        auto &leg = legs[i];
        bool groupA = (i % 2 == 0);

        vec3 idealWorldPos = vec3(transform.to_matrix() * vec4(leg.rest_position, 1.0f));

        if (leg.step_t >= 1.0f)
        {
            leg.step_cooldown -= delta_time;
            float drift = length(idealWorldPos - leg.current_target);

            bool staggerOk = groupA || (animTime - lastGroupAStepTime > GROUP_STAGGER);

            if (drift > STEP_TRIGGER_DIST && leg.step_cooldown <= 0.0f && staggerOk)
            {
                leg.step_start = leg.current_target;
                leg.step_end = idealWorldPos;
                leg.step_t = 0.0f;
                leg.step_cooldown = 0.25f;

                if (groupA)
                    lastGroupAStepTime = animTime;
            }
        }
        else
        {
            leg.step_t += delta_time * STEP_SPEED;
            leg.step_t = std::min(leg.step_t, 1.0f);

            vec3 flat = mix(leg.step_start, leg.step_end, leg.step_t);
            float lift = sinf(leg.step_t * 3.14159f) * STEP_HEIGHT;
            leg.current_target = flat + vec3(0, lift, 0);
        }

        solve_leg_ik(leg, leg.current_target, delta_time);
    }
}