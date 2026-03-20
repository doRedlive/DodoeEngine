//
// Minimal camera API placeholder for current runtime integration.
//

#ifndef DODOE_CAMERA_H
#define DODOE_CAMERA_H

struct GLFWwindow;

namespace dodoe {

class Camera {
public:
    static void initialize(GLFWwindow*) {}
};

} // namespace dodoe

#endif // DODOE_CAMERA_H
