//---------------------------------------------------------------------------------------------------------------------
//  @file   compute_device.cpp
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

#include "compute_device.h"

#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QVector>
#if QT_CONFIG(vulkan)
#include <QVulkanInstance>
#endif
#include <rhi/qrhi.h>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
// The APIs to try, in turn, for one asked for.
QVector<ComputeDevice::Api> candidates(ComputeDevice::Api api)
{
    if (api != ComputeDevice::Api::Platform)
    {
        return {api};
    }
#if defined(Q_OS_WIN)
    return {ComputeDevice::Api::Direct3D11};
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    return {ComputeDevice::Api::Metal};
#else
    return {ComputeDevice::Api::Vulkan, ComputeDevice::Api::OpenGL};
#endif
}

//---------------------------------------------------------------------------------------------------------------------
// OpenGL computes from version 4.3 on.
QSurfaceFormat computeFormat()
{
    QSurfaceFormat format;
    format.setVersion(4, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    return format;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
/// @brief On the GUI thread: sets up what the API needs there.
ComputeDevice::ComputeDevice(Api api)
    : m_api(api)
    , m_surface()
    , m_vulkan()
    , m_rhi()
{
    const QVector<Api> apis = candidates(api);
#if QT_CONFIG(opengl)
    if (apis.contains(Api::OpenGL))
    {
        m_surface.reset(QRhiGles2InitParams::newFallbackSurface(computeFormat()));
    }
#endif
#if QT_CONFIG(vulkan)
    if (apis.contains(Api::Vulkan))
    {
        m_vulkan.reset(new QVulkanInstance());
        m_vulkan->setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
        if (!m_vulkan->create())
        {
            m_vulkan.reset();
        }
    }
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief On the GUI thread, once the device is closed.
ComputeDevice::~ComputeDevice()
{
    close();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief On the thread that computes: opens the device, if it can compute. Null if there is none that can.
QRhi* ComputeDevice::open()
{
    if (m_rhi == nullptr)
    {
        for (const Api api : candidates(m_api))
        {
            if (create(api) != nullptr)
            {
                break;
            }
        }
    }
    return m_rhi.get();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief On the thread that opened it, once everything computing on it is gone.
void ComputeDevice::close()
{
    m_rhi.reset();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The open device, or null.
QRhi* ComputeDevice::rhi() const
{
    return m_rhi.get();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief The graphics API and the card, as the driver names them, once open.
QString ComputeDevice::name() const
{
    return m_rhi != nullptr ? QStringLiteral("%1, %2").arg(QString::fromLatin1(m_rhi->backendName()),
                                                          QString::fromUtf8(m_rhi->driverInfo().deviceName))
                            : QString();
}

//---------------------------------------------------------------------------------------------------------------------
// Opens the device through one API; it is kept if it can compute.
QRhi* ComputeDevice::create(Api api)
{
    std::unique_ptr<QRhi> rhi;
    switch (api)
    {
        case Api::Direct3D11:
        case Api::Software:
        {
#if defined(Q_OS_WIN)
            QRhiD3D11InitParams params;
            rhi.reset(QRhi::create(QRhi::D3D11, &params,
                                   api == Api::Software ? QRhi::PreferSoftwareRenderer : QRhi::Flags()));
#endif
            break;
        }
        case Api::Direct3D12:
        {
#if defined(Q_OS_WIN)
            QRhiD3D12InitParams params;
            rhi.reset(QRhi::create(QRhi::D3D12, &params));
#endif
            break;
        }
        case Api::Metal:
        {
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
            QRhiMetalInitParams params;
            rhi.reset(QRhi::create(QRhi::Metal, &params));
#endif
            break;
        }
        case Api::Vulkan:
        {
#if QT_CONFIG(vulkan)
            if (m_vulkan != nullptr)
            {
                QRhiVulkanInitParams params;
                params.inst = m_vulkan.get();
                rhi.reset(QRhi::create(QRhi::Vulkan, &params));
            }
#endif
            break;
        }
        case Api::OpenGL:
        {
#if QT_CONFIG(opengl)
            if (m_surface != nullptr)
            {
                QRhiGles2InitParams params;
                params.format = computeFormat();
                params.fallbackSurface = m_surface.get();
                rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
            }
#endif
            break;
        }
        case Api::Platform:
        default:
            break;
    }

    if (rhi != nullptr && rhi->isFeatureSupported(QRhi::Compute))
    {
        m_rhi = std::move(rhi);
    }
    return m_rhi.get();
}
