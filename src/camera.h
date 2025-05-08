
#include <vk_types.h>
#include <SDL_events.h>

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    // vertical rotation
    float pitch = 0.0f;
    // horizontal rotation
    float yaw = 0.0f;
    float speedModifier = 1.0f;
    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void ProcessSDLEvent(SDL_Event& e, float deltaTime);

    void Update(float deltaTime);
};
