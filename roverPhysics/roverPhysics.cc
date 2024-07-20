#include "roverPhysics.h"

#include <dlfcn.h>
#include <mujoco/mujoco.h>
#include <stdio.h>

extern "C" void roverPhysicsTestPlug(void) { printf("Hi from Rover!\n"); }

extern "C" int roverPhysicsInitPlug(mjModel *m, mjData *d) {
  printf("Init Rover Plug!\n");
  return 0;
}

extern "C" bool roverPhysicsUpdatePlug(mjModel *m, mjData *d) {
    // printf("Physics Plug!\n");
    // printf("Updated Physics Plug!\n");
    float water_gain = 470.0f;
    float volume_displaced = 0.4572 *0.33782 *0.254;
    float max_bouyancy_force = 9.806f * volume_displaced * 1000;
    float start_height = 1;
    float force = (start_height - d->qpos[2]) * water_gain;

    if(d->qpos[2] <= start_height - 0.1 ){
        float fnew = (force < max_bouyancy_force) ? (force) : (max_bouyancy_force);
        d->qfrc_applied[2] = fnew;
        // printf("UP:%0.4f | MAX_B: %0.4f\n",fnew, max_bouyancy_force);
    }
    if(d->qpos[2] >= start_height + 0.1 ){
        d->qfrc_applied[2] = (1 - d->qpos[2]) * water_gain;
        // printf("Down:%0.4f | MAX_B: %0.4f\n",force, max_bouyancy_force);
    }

    // TODO: Create figure to show forces
  return true;
}
