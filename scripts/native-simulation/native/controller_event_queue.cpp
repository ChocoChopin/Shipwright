#define SDL_MAIN_HANDLED
#include "ship/controller/physicaldevice/SDLControllerEventQueue.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

static unsigned checks;
static void Check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
    ++checks;
}

static int Pending() {
    return SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
}

int main() {
    SDL_SetMainReady();
    try {
        Check(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0, "initialize SDL");
        const int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                                     SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        Check(device >= 0, "attach process-local virtual controller");
        SDL_GameController* controller = SDL_GameControllerOpen(device);
        Check(controller != nullptr, "open virtual controller");
        SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
        Check(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, 20000) == 0,
              "set virtual axis");
        Check(SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1) == 0,
              "set virtual button");
        SDL_PumpEvents();
        Check(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) == 20000 &&
                  SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) == 1,
              "fresh state before draining");
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);

        // A bounded backlog like the one the native window used to accumulate.
        std::vector<SDL_Event> events(20000);
        for (size_t i = 0; i < events.size(); ++i) {
            events[i].type = i % 2 ? SDL_JOYAXISMOTION : SDL_CONTROLLERAXISMOTION;
        }
        Check(SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_ADDEVENT, 0, 0) == 20000,
              "enqueue motion backlog");
        SDL_Event event{};
        SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEADDED);
        SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEREMOVED, SDL_CONTROLLERDEVICEREMOVED);
        Check(Pending() == 20000, "legacy hotplug queries leave the motion backlog");
        const Uint32 retainedTypes[] = { SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED,
                                         SDL_KEYDOWN, SDL_USEREVENT };
        for (const auto type : retainedTypes) {
            event = {};
            event.type = type;
            Check(SDL_PeepEvents(&event, 1, SDL_ADDEVENT, 0, 0) == 1, "enqueue retained event");
        }
        Ship::DiscardNativeWindowControllerNotifications();
        Check(Pending() == 4, "only owned notifications removed");
        for (const auto type : retainedTypes) {
            Check(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1 &&
                      event.type == type, "hotplug and unrelated event order retained");
        }
        Check(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) == 20000 &&
                  SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) == 1,
              "draining does not consume or reset live controller state");

        SDL_Event notifications[8]{};
        const Uint32 types[] = { SDL_JOYAXISMOTION, SDL_JOYHATMOTION, SDL_JOYDEVICEADDED,
                                SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONDOWN,
                                SDL_CONTROLLERDEVICEREMAPPED, SDL_CONTROLLERTOUCHPADMOTION,
                                SDL_CONTROLLERSENSORUPDATE };
        for (int i = 0; i < 8; ++i) notifications[i].type = types[i];
        for (int frame = 0; frame < 7200; ++frame) {
            if (SDL_PeepEvents(notifications, 8, SDL_ADDEVENT, 0, 0) != 8)
                throw std::runtime_error("enqueue frame notifications");
            Ship::DiscardNativeWindowControllerNotifications();
            if (Pending() != 0) throw std::runtime_error("queue grows across frames");
        }
        ++checks;
        Check(SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, -12000) == 0 &&
                  SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 0) == 0,
              "set following input sample");
        SDL_PumpEvents();
        Ship::DiscardNativeWindowControllerNotifications();
        Check(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) == -12000 &&
                  SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) == 0,
              "following sample is immediately readable");
        SDL_GameControllerClose(controller);
        Check(SDL_JoystickDetachVirtual(device) == 0, "detach virtual controller");
        SDL_Quit();
        std::printf("PASS: %u checks; 20000-event backlog; 7200 bounded queue drains; live axis/button state preserved\n",
                    checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s (SDL: %s)\n", error.what(), SDL_GetError());
        SDL_Quit();
        return 2;
    }
}
