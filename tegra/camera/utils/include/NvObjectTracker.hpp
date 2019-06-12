/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVOBJECTTRACKER_HPP_
#define NVOBJECTTRACKER_HPP_

class NvObjectTrackerPrivate;

class NvObjectTracker
{
public:
    NvObjectTracker();
    ~NvObjectTracker();

    enum StatusCode
    {
        OBJECT_TRACKED = 1,
        OBJECT_NOT_TRACKED
    };

    //  x,y = upper left corner
    //  w,h = width in x and height in y
    typedef struct TrackingRectangleRec
    {
        int x;
        int y;
        int w;
        int h;
    } TrackingRectangle;

    // Set rectangle to track in preview image coordinates.
    // Once a rectangle is set, it will be tracked for as long as possible, even
    // if the object moves out of frame.
    void setTrackingRectangle(TrackingRectangle rect);

    // Stop tracking the current rectangle.
    // No new coordinates will be returned until a new rectangle is set.
    void clearTrackingRectangle();

    // Provide a new frame to the tracker and return updated tracking rectangle.
    // This should be called as fast as possible by the camera preview thread.
    // The returned status code indicates whether the data in pRect has been updated with
    // new object coordinates when the object is in frame.
    // The UI can use these coordinates to highlight the object.
    //
    // pImage is a pitch linear, 8bit greyscale image of width x height pixels
    // pRect is returned with pixel coordinates of the tracked object wrt the input image
    StatusCode trackNewFrame(unsigned char* pImage, int width, int height, TrackingRectangle* pRect);

private:
    NvObjectTrackerPrivate* pd;
};

#endif //NVOBJECTTRACKER_HPP_
