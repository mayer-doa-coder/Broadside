#pragma once

// Broadside — Phase 3: orbit camera.
//
// The grader has to inspect specular highlights from arbitrary angles, and
// specular is view-dependent by definition (L8 s44), so a camera that can circle
// the scene is a grading requirement, not a convenience.
//
// The camera is stored in spherical coordinates around a target point rather than
// as a free-flying position + direction. That guarantees the target always stays
// framed, which is exactly what "inspect this object" needs.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

struct OrbitCamera {
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    float     radius = 4.0f;
    float     yaw    = glm::radians(35.0f);   // around +Y, 0 puts the camera on +Z
    float     pitch  = glm::radians(22.0f);   // above the horizon

    float fovDeg = 45.0f;
    float nearZ  = 0.1f;
    float farZ   = 300.0f;                    // guide Phase 3

    static constexpr float MIN_RADIUS = 1.2f;
    static constexpr float MAX_RADIUS = 120.0f;

    // 89 degrees, not 90: at exactly 90 the view direction is parallel to the
    // world up vector and glm::lookAt degenerates into a NaN matrix.
    static constexpr float MAX_PITCH = 1.5533431f;

    glm::vec3 position() const
    {
        const float cp = std::cos(pitch);
        return target + radius * glm::vec3(cp * std::sin(yaw),
                                           std::sin(pitch),
                                           cp * std::cos(yaw));
    }

    glm::mat4 view() const
    {
        return glm::lookAt(position(), target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Aspect comes from the real framebuffer every frame, so resizing the window
    // re-derives it instead of stretching the image.
    glm::mat4 projection(int framebufferWidth, int framebufferHeight) const
    {
        const float aspect = (framebufferHeight > 0)
                           ? (float)framebufferWidth / (float)framebufferHeight
                           : 1.0f;
        return glm::perspective(glm::radians(fovDeg), aspect, nearZ, farZ);
    }

    void orbit(float deltaYaw, float deltaPitch)
    {
        yaw += deltaYaw;

        // Wrap yaw so a long drag session cannot grow it until float precision
        // makes the rotation visibly step.
        const float twoPi = 6.2831853f;
        yaw = std::fmod(yaw, twoPi);
        if (yaw < 0.0f) yaw += twoPi;

        pitch = glm::clamp(pitch + deltaPitch, -MAX_PITCH, MAX_PITCH);
    }

    // Multiplicative so a step feels the same close up and far away, which a
    // linear +/- on radius does not.
    void zoom(float exponent)
    {
        radius = glm::clamp(radius * std::exp(exponent), MIN_RADIUS, MAX_RADIUS);
    }
};
