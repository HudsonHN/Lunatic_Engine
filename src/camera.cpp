#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::Update(float deltaTime)
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * speedModifier, 0.0f)) * deltaTime;
}

void Camera::ProcessSDLEvent(SDL_Event& e, float deltaTime)
{
    // Apply speed if key is pressed
    if (e.type == SDL_KEYDOWN)
    {
        if (e.key.keysym.sym == SDLK_LSHIFT) { speedModifier = 2.0f; }
        if (e.key.keysym.sym == SDLK_LCTRL) { speedModifier = 0.0625f; }
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -0.05f; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0.05f; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -0.05f; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0.05f; }
        if (e.key.keysym.sym == SDLK_q) { velocity.y = -0.05f; }
        if (e.key.keysym.sym == SDLK_e) { velocity.y = 0.05f; }
    }

    // Reset speed if key is released
    if (e.type == SDL_KEYUP)
    {
        if (e.key.keysym.sym == SDLK_LSHIFT) { speedModifier = 0.5f; }
        if (e.key.keysym.sym == SDLK_LCTRL) { speedModifier = 0.5f; }
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_q) { velocity.y = 0; }
        if (e.key.keysym.sym == SDLK_e) { velocity.y = 0; }
    }

    if (e.type == SDL_MOUSEMOTION)
    {
        yaw += static_cast<float>(e.motion.xrel) * deltaTime / 1000.0f;
        float tempPitch = pitch - static_cast<float>(e.motion.yrel) * deltaTime / 1000.0f;
        if (tempPitch >= -1.5f && tempPitch <= 1.5f)
        {
            pitch -= static_cast<float>(e.motion.yrel) * deltaTime / 1000.0f;
        }
    }

    if (e.type == SDL_MOUSEBUTTONUP)
    {
        if (e.button.button == SDL_BUTTON_RIGHT)
        {
            velocity = glm::vec3{0.0f};
        }
    }

}

glm::mat4 Camera::getViewMatrix()
{
    // To create a correct model view, we need to move the world in opposite
    // direction to the camera, so we'll create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // Typical FPS-style camera, joining pitch and yaw rotations into final rot matrix
    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.0f, 0.0f, 0.0f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.0f, -1.0f, 0.0f }); // Rotate yaw around -Y to make sure positive yaw is 
                                                                                    // to the right, which is more natural for FPS
    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}