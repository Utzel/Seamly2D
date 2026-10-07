//---------------------------------------------------------------------------------------------------------------------
//  @file   compute_device.h
//  @author Julius
//  @date   7 Oct, 2026
//
//  @copyright
//  Copyright (C)  2026 Seamly, LLC
//  https://github.com/fashionfreedom/seamly2d
//
//  @brief
//  Seamly2D is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Seamly2D is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Seamly2D. If not, see <http://www.gnu.org/licenses/>.
//---------------------------------------------------------------------------------------------------------------------

#ifndef COMPUTE_DEVICE_H
#define COMPUTE_DEVICE_H

#include <QString>
#include <QtGlobal>

#include <memory>

class QOffscreenSurface;
class QRhi;
class QVulkanInstance;

/// @brief The graphics card, to compute on, through Qt's rendering hardware interface.
///
/// Each platform has its own graphics API: Direct3D on Windows, Metal on macOS, Vulkan or else OpenGL elsewhere. Some
/// need things set up on the GUI thread, which the device does when it is made; the work itself can run on another
/// thread, which opens the device, computes and closes it again.
class ComputeDevice
{
public:
    enum class Api : quint8
    {
        Platform,  ///< the usual one on this platform
        Direct3D11,
        Direct3D12,
        Metal,
        Vulkan,
        OpenGL,
        Software   ///< the processor, through Direct3D 11's software adapter on Windows; for tests
    };

    explicit           ComputeDevice(Api api = Api::Platform);
                       ~ComputeDevice();

    QRhi*              open();
    void               close();
    QRhi*              rhi() const;
    QString            name() const;

private:
    Q_DISABLE_COPY(ComputeDevice)

    Api                                m_api;
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QVulkanInstance>   m_vulkan;
    std::unique_ptr<QRhi>              m_rhi;

    QRhi*              create(Api api);
};

#endif // COMPUTE_DEVICE_H
