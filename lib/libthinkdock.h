/*
 * Copyright (c) 2017 Ognjen Galić
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LIBTHINKDOCK_LIBRARY_H
#define LIBTHINKDOCK_LIBRARY_H

#include "config.h"

#include <string>
#include <vector>
#include <memory>
#include <cstdio>

#include <X11/extensions/Xrandr.h>

#ifdef SYSTEMD
#include <systemd/sd-bus.h>
#endif

#define IBM_DOCK_DOCKED     "/sys/devices/platform/dock.2/docked"
#define IBM_DOCK_MODALIAS   "/sys/devices/platform/dock.2/modalias"
#define IBM_DOCK_ID         "acpi:IBM0079:PNP0C15:LNXDOCK:\n"
#define ERR_INVALID (-1)

#define SUSPEND_REASON_LID 0
#define SUSPEND_REASON_BUTTON 1

using std::string;
using std::vector;

typedef RRMode VideoOutputModeType;
typedef RROutput VideoOutputType;
typedef RRCrtc VideoControllerType;
typedef XRROutputInfo VideoOutputInfo;
typedef XRRModeInfo VideoOutputModeInfo;
typedef XRRCrtcInfo VideoControllerInfo;

typedef int SUSPEND_REASON;
typedef int STATUS;

namespace ThinkDock {

    /**
     * The Dock class is used to probe for the dock
     * validity and probe for basic information about the dock.
     */
    class Dock {

    public:

        /**
         * Check if the ThinkPad is physically docked
         * into the UltraDock or the UltraBase
         * @return true if the ThinkPad is inside the dock
         */
        bool isDocked();

        /**
         * Probes the dock if it is an IBM dock and if the
         * dock is sane and ready for detection/state changes
         * @return true if the dock is sane and valid
         */
        bool probe();

    };

    /**
     * The ThinkPad power manager. This handles the
     * system power state and ACPI event dispatches.
     */
    class PowerManager {
    private:
        static bool suspend();
    public:
        /**
         * Request a suspend of the system. You need to specify a suspend
         * reason, whether it is the lid or the user pressing the button.
         * This should be called in an ACPI event handler.
         * @param reason the reason for the suspend, lid or button
         * @return true if the suspend was successful
         */
        static bool requestSuspend(SUSPEND_REASON reason);
    };

    namespace DisplayManager {

        class XServer;
        class ScreenResources;
        class VideoOutput;
        class VideoController;
        class VideoOutputMode;
        class Monitor;
        class ConfigurationManager;
        typedef struct _point point;
        typedef struct _dimensions dimensions;

        /**
         * The VideoOutputMode class defines a mode for outputting
         * to a VideoOutput. All modes are common for all outputs, so
         * any mode can theoretically be set to any output.
         *
         * Defined parameters are resolution, refresh rate and output
         * mode (Interlanced/DoubleScan).
         */
        class VideoOutputMode {
        private:
            VideoOutputModeType id;
            VideoOutputModeInfo *info;
            ScreenResources *parent;
            string *name;
        public:
            VideoOutputMode(XRRModeInfo *modeInfo, ScreenResources *resources);
            ~VideoOutputMode();
            string *getName() const;
            VideoOutputModeType getOutputModeId() const;

            /**
             * Returns the ACTUAL refresh rate of the monitor, not
             * the doubled (DoubleScan) or halved (Interlanced)
             * refresh rate.
             * @return  the actual refresh rate of the monitor
             */
            double getRefreshRate() const;
            string toString() const;
            unsigned int getWidthPixels();
            unsigned int getHeightPixels();
        };

        /**
         * The VideoOutput class represents a physical output __PORT__,
         * NOT an actual device connected. The information about the
         * VideoOutput such as the height and width in millimeters, is fetched
         * for the current device CONNECTED to the port.
         */
        class VideoOutput {
        private:
            VideoOutputType *id;
            VideoOutputInfo *info;
            ScreenResources *parent;
            string *name;
        public:
            ~VideoOutput();
            VideoOutput(VideoOutputType *type, ScreenResources *resources);
            bool isConnected() const;

            /**
             * Return the visual representation of the output mode,
             * to be displayed on graphical interfaces.
             * For ex.: "640x480"
             * @return the textual representation of the output mode
             */
            string* getName() const;

            VideoOutputType *getOutputId() const;

            /**
             * The preferred output mode is usually the monitors native
             * resolution and a refresh rate of 60Hz, even if monitors
             * support higher refresh rates.
             * @return the preffered video mode
             */
            VideoOutputMode* getPreferredOutputMode() const;

            bool isControllerSupported(VideoController *pController);

            void setController(VideoController *pController);

            unsigned long getWidthMillimeters() const;
            unsigned long getHeightMillimeters() const;
        };

        /**
         * A graphics processing unit has so-called Video Display Controllers
         * (or VDC for short) that are image drivers for data from the GPU.
         * The VDC takes the data from the GPU and encodes it in an abstract format
         * that defines position, resolution and refresh rate.
         *
         * A GPU can drive only up to n-available VDCs, which means that a GPU
         * can't output more unique images than there are VDCs.
         *
         * However, a VDC can mirror the data to another output.
         *
         * For example, the Lenovo ThinkPad X220 has 2 VDC's and 8 outputs.
         * That means that the X220 can display up to 2 unqiue images that define
         * resolution, position and refresh rate, but those 2 images can be mirrored
         * and displayed on up-to 8 outputs.
         *
         * If you connect a projector to the VGA port and mirror the ThinkPad
         * screen to that, we are only using a single VDC to drive two outputs.
         * It is favorable for the outputs to share a common output mode that
         * both displays support, for ex. 800x600.
         *
         * The second VDC is not used and is disabled.
         *
         *           +-----------+     +-----------+
         *           |    VDC    |     |    VDC    |
         *           |  800x600  |     |    off    |
         *           |   59 Hz   |     |           |
         *           +-----+-----+     +-----------+
         *                 |
         *         +-------+-------+
         *         |               |
         *   +-----+-----+   +-----+-----+
         *   | ThinkPad  |   | Projector |
         *   |  Screen   |   |           |
         *   +-----------+   +-----------+
         *
         * However, if you connect a screen to the and "extend" the display area,
         * you are now using 2 VDC's and each VFC has its parameters defined, such
         * as resolution, position and refresh rate. The output modes can, but are
         * usually not common.
         *
         *          +-----------+     +-----------+
         *          |    VDC    |     |    VDC    |
         *          | 1366x768  |     | 1440x900  |
         *          |   59 Hz   |     |   75 Hz   |
         *          +-----+-----+     +-----+-----+
         *                |                 |
         *                |                 |
         *          +-----+-----+     +-----+-----+
         *          | ThinkPad  |     | External  |
         *          |  Screen   |     | Monitor   |
         *          +-----------+     +-----------+
         *
         * If running in "mirror" mode, each VDC has its position set to 0,0.
         * If running in "extend" mode, each VDC has its position set relative to the
         * primary monitor, which is considered the monitor positioned to 0,0.
         *
         */
        class VideoController {
        private:
            VideoControllerType *id;
            VideoControllerInfo *info;
            ScreenResources *parent;
            vector<VideoOutput*> *activeOutputs;
            vector<VideoOutput*> *supportedOutputs;
        public:
            ~VideoController();
            VideoController(VideoControllerType *id, ScreenResources *resources);

            /**
             * Get the currently active outputs on the video controller.
             * @return a list of active video outputs
             */
            vector<VideoOutput*> *getActiveOutputs();
            VideoControllerType* getControllerId() const;

            /*
             * The X and Y  positions determine relations between monitors.
             * The primary monitor is always located at 0.0.
             */

            /**
             * Gets the virtual Y position on the display pane.
             * @return the virtual Y position
             */
            int getXPosition() const;
            int getYPosition() const;

            /**
             * Gets the virtual T position on the display pane.
             * @return the virtual Y position
             */

            void setPosition(point position);
            void setWidthPixels(unsigned int param);
            void seHeightPixels(unsigned int param);

            void setOutputMode(VideoOutputMode *pMode);
            void addOutput(VideoOutput *output);

            bool resetConfiguration();

            bool isEnabled() const;

            vector<VideoOutput*> *getSupportedOutputs();
        };

        /**
         * The ScreenResources class is actually the main container
         * of all the objects used by the display management system.
         * The ScreenResources class contains all the output modes, the
         * controllers and the physical outputs.
         */
        class ScreenResources {
        private:
            XRRScreenResources *resources;
            XServer *parentServer;
            vector<VideoController*> *controllers;
            vector<VideoOutput*> *videoOutputs;
            vector<VideoOutputMode*> *videoOutputModes;
        public:

            ScreenResources(XServer *server);
            ~ScreenResources();

            vector<VideoController*> *getControllers() const;
            vector<VideoOutput*> *getVideoOutputs() const;
            vector<VideoOutputMode*> *getVideoOutputModes() const;

            vector<DisplayManager::VideoOutput *> *getConnectedOutputs();

            XRRScreenResources *getRawResources();
            XServer *getParentServer() const;

        };

        /**
         * The XServer class is the main connection and relation
         * to the X server running on 0:0, localhost.
         */
        class XServer {
        private:
            Display *display;
            int screen;
            Window window;
            static XServer *server;
        public:
            ~XServer();
            bool connect();

            /**
             * The connection to the X server is a singleton
             * @return the singleton XServer instance
             */
            static XServer* getDefaultXServer();
            Display *getDisplay();
            Window getWindow();
        };

        class Monitor {
        private:

            VideoOutputMode *videoMode = nullptr;
            VideoController *videoController = nullptr;
            VideoOutput *videoOutput = nullptr;

            Monitor *topWing;
            Monitor *leftWing;
            Monitor *rightWing;
            Monitor *bottomWing;

            unsigned int xAxisMaxHeight = 0;
            unsigned int yAxisMaxWidth = 0;

            unsigned long xAxisMaxHeightmm = 0;
            unsigned long yAxisMaxWidthmm = 0;

            bool limitsCalculated = false;

            void calculateLimits();

        public:

            void setOutput(VideoOutput *output);
            void setOutputMode(VideoOutputMode *mode);
            bool setController(VideoController *pController);
            bool isControllerSupported(VideoController *pController);
            void disable(ScreenResources *pResources);

            string* getName();
            VideoOutputMode *getPreferredOutputMode() const;
            VideoOutput* getOutput();

            void setRightWing(Monitor*);
            void setLeftWing(Monitor*);
            void setTopWing(Monitor *);
            void setBottomWing(Monitor *);

            unsigned int getTotalWidth();
            unsigned int getTotalHeight();

            point getPrimaryPosition();
            void calculateMonitorPositions();
            dimensions getScreenDimensionsPixels();
            dimensions getScreenDimensionsMillimeters();

            void applyCascadingConfig(ScreenResources *pResources);

            void setConfig(Monitor *pMonitor, ScreenResources *pResources);
        };

        class ConfigurationManager {
        private:
            vector<Monitor*>* allMonitors;
            Monitor *primaryMonitor;
            ScreenResources *resources;
        public:

            ConfigurationManager(ScreenResources *resources);
            ~ConfigurationManager();

            void setMonitorPrimary(Monitor *monitor);
            vector<DisplayManager::Monitor *> *getAllMonitors();

            void commit();

        };

        typedef struct _point {
            unsigned int x;
            unsigned int y;
        } point;

        typedef struct _dimensions {
            unsigned long width;
            unsigned long height;
        } dimensions;

    };

};


#endif