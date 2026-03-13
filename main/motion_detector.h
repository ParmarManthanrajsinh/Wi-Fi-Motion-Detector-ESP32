#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <stdbool.h>

// Initialize the motion detection background task.
void motion_detector_init(void);

// Getters for the web server to access the variables
int motion_detector_get_current_rssi(void);
float motion_detector_get_baseline_rssi(void);
bool motion_detector_is_motion_detected(void);
bool motion_detector_is_calibrating(void);

#endif // MOTION_DETECTOR_H
