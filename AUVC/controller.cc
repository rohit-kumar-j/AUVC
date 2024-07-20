#include "controller.h"
#include "mujoco/mjmodel.h"

#include <dlfcn.h>
#include <mujoco/mujoco.h>
#include <stdio.h>

static double Qorn[4] ={0}; // w,x,y,z
static double orn[3] ={0}; // r[forward axis],p[possibly side axis],y[vertical axis]

void quaternion_to_euler(double x, double y, double z, double w, double *X, double *Y, double *Z) {
    double t0 = +2.0 * (w * x + y * z);
    double t1 = +1.0 - 2.0 * (x * x + y * y);
    *X = atan2(t0, t1) * (180.0 / M_PI);

    double t2 = +2.0 * (w * y - z * x);
    t2 = +1.0 > t2 ? +1.0 : t2;
    t2 = -1.0 < t2 ? -1.0 : t2;
    *Y = asin(t2) * (180.0 / M_PI);

    double t3 = +2.0 * (w * z + x * y);
    double t4 = +1.0 - 2.0 * (y * y + z * z);
    *Z = atan2(t3, t4) * (180.0 / M_PI);
}

extern "C" void controllerTestFunc(void) { printf("Hi from Plugin!\n"); }

extern "C" int controllerInitPlug(mjModel *m, mjData *d) {
  printf("Init Plug!\n");
  return 0;
}

extern "C" bool controllerUpdatePlug(mjModel *m, mjData *d) {
    int idx = mj_name2id(m, mjOBJ_ACTUATOR , "a6");
    // d->ctrl[0] = 100; // a1
    // d->ctrl[1] = 100; // a2
    // d->ctrl[2] = -100; // a3
    // d->ctrl[3] = 100; // a4
    // d->ctrl[4] = -100; // a5
    // d->ctrl[5] = 100; // a6
    // Capture Orientation
    // idx = 3; // [Px,Py,Pz, Ow,Ox,Oy,Oz]
    //     Orientation:    ^_________^
    for(int i =0; i<4; i++){
      Qorn[i] = d->sensordata[3 + i];
    }

   quaternion_to_euler(Qorn[1],Qorn[2],Qorn[3],Qorn[0], &orn[0], &orn[1], &orn[2]);
   printf("[0]: %f | [1]: %f | [2]: %f\n",orn[0], orn[1],orn[2]);

  return true;
}
