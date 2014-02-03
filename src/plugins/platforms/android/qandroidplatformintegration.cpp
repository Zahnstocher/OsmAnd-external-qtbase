/****************************************************************************
**
** Copyright (C) 2012 BogDan Vatra <bogdan@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qandroidplatformintegration.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QThread>

#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>

#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

#include "androidjnimain.h"
#include "qabstracteventdispatcher.h"
#include "qandroidplatformaccessibility.h"
#include "qandroidplatformclipboard.h"
#include "qandroidplatformfontdatabase.h"
#include "qandroidplatformbackingstore.h"
#include "qandroidplatformopenglcontext.h"
#include "qandroidplatformopenglwindow.h"
#include "qandroidplatformrasterwindow.h"
#include "qandroidplatformscreen.h"
#include "qandroidplatformservices.h"
#include "qandroidplatformtheme.h"
#include "qandroidsystemlocale.h"


QT_BEGIN_NAMESPACE

int QAndroidPlatformIntegration::m_defaultGeometryWidth = 320;
int QAndroidPlatformIntegration::m_defaultGeometryHeight = 455;
int QAndroidPlatformIntegration::m_defaultPhysicalSizeWidth = 50;
int QAndroidPlatformIntegration::m_defaultPhysicalSizeHeight = 71;

Qt::ScreenOrientation QAndroidPlatformIntegration::m_orientation = Qt::PrimaryOrientation;
Qt::ScreenOrientation QAndroidPlatformIntegration::m_nativeOrientation = Qt::PrimaryOrientation;

void *QAndroidPlatformNativeInterface::nativeResourceForIntegration(const QByteArray &resource)
{
    if (resource=="JavaVM")
        return QtAndroid::javaVM();
    if (resource == "QtActivity")
        return QtAndroid::activity();
    if (resource == "AndroidStylePalettes")
        return &m_palettes;
    if (resource == "AndroidStyleFonts")
        return &m_fonts;
    return 0;
}

QAndroidPlatformIntegration::QAndroidPlatformIntegration(const QStringList &paramList)
    : m_touchDevice(0)
#ifndef QT_NO_ACCESSIBILITY
    , m_accessibility(0)
#endif
{
    Q_UNUSED(paramList);

    m_androidPlatformNativeInterface = new QAndroidPlatformNativeInterface();

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        qFatal("Could not bind GL_ES API");

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY)
        qFatal("Could not open egl display");

    EGLint major, minor;
    if (!eglInitialize(m_eglDisplay, &major, &minor))
        qFatal("Could not initialize egl display");

    m_primaryScreen = new QAndroidPlatformScreen();
    screenAdded(m_primaryScreen);
    m_primaryScreen->setPhysicalSize(QSize(m_defaultPhysicalSizeWidth, m_defaultPhysicalSizeHeight));
    m_primaryScreen->setGeometry(QRect(0, 0, m_defaultGeometryWidth, m_defaultGeometryHeight));

    m_mainThread = QThread::currentThread();
    QtAndroid::setAndroidPlatformIntegration(this);

    m_androidFDB = new QAndroidPlatformFontDatabase();
    m_androidPlatformServices = new QAndroidPlatformServices();
    m_androidPlatformClipboard = new QAndroidPlatformClipboard();

    m_androidSystemLocale = new QAndroidSystemLocale;
}

bool QAndroidPlatformIntegration::hasCapability(Capability cap) const
{
    switch (cap) {
        case ThreadedPixmaps: return true;
        case ApplicationState: return true;
        case NativeWidgets: return true;
        case OpenGL: return true;
        case ThreadedOpenGL: return true;
        default:
            return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformBackingStore *QAndroidPlatformIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QAndroidPlatformBackingStore(window);
}

QPlatformOpenGLContext *QAndroidPlatformIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    QSurfaceFormat format(context->format());
    format.setAlphaBufferSize(8);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    return new QAndroidPlatformOpenGLContext(format, context->shareHandle(), m_eglDisplay);
}

QPlatformWindow *QAndroidPlatformIntegration::createPlatformWindow(QWindow *window) const
{
    if (window->surfaceType() == QSurface::RasterSurface)
        return new QAndroidPlatformRasterWindow(window);
    else
        return new QAndroidPlatformOpenGLWindow(window, m_eglDisplay);
}

QAbstractEventDispatcher *QAndroidPlatformIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QAndroidPlatformIntegration::~QAndroidPlatformIntegration()
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
        eglTerminate(m_eglDisplay);

    delete m_androidPlatformNativeInterface;
    delete m_androidFDB;
    delete m_androidSystemLocale;
    QtAndroid::setAndroidPlatformIntegration(NULL);
}

QPlatformFontDatabase *QAndroidPlatformIntegration::fontDatabase() const
{
    return m_androidFDB;
}

#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QAndroidPlatformIntegration::clipboard() const
{
static QAndroidPlatformClipboard *clipboard = 0;
    if (!clipboard)
        clipboard = new QAndroidPlatformClipboard;

    return clipboard;
}
#endif

QPlatformInputContext *QAndroidPlatformIntegration::inputContext() const
{
    return &m_platformInputContext;
}

QPlatformNativeInterface *QAndroidPlatformIntegration::nativeInterface() const
{
    return m_androidPlatformNativeInterface;
}

QPlatformServices *QAndroidPlatformIntegration::services() const
{
    return m_androidPlatformServices;
}

QVariant QAndroidPlatformIntegration::styleHint(StyleHint hint) const
{
    switch (hint) {
    case ShowIsMaximized:
        return true;
    default:
        return QPlatformIntegration::styleHint(hint);
    }
}

Qt::WindowState QAndroidPlatformIntegration::defaultWindowState(Qt::WindowFlags flags) const
{
    // Don't maximize dialogs on Android
    if (flags & Qt::Dialog & ~Qt::Window)
        return Qt::WindowNoState;

    return QPlatformIntegration::defaultWindowState(flags);
}

static const QLatin1String androidThemeName("android");
QStringList QAndroidPlatformIntegration::themeNames() const
{
    return QStringList(QString(androidThemeName));
}

QPlatformTheme *QAndroidPlatformIntegration::createPlatformTheme(const QString &name) const
{
    if (androidThemeName == name)
        return new QAndroidPlatformTheme(m_androidPlatformNativeInterface);

    return 0;
}

void QAndroidPlatformIntegration::setDefaultDisplayMetrics(int gw, int gh, int sw, int sh)
{
    m_defaultGeometryWidth = gw;
    m_defaultGeometryHeight = gh;
    m_defaultPhysicalSizeWidth = sw;
    m_defaultPhysicalSizeHeight = sh;
}

void QAndroidPlatformIntegration::setDefaultDesktopSize(int gw, int gh)
{
    m_defaultGeometryWidth = gw;
    m_defaultGeometryHeight = gh;
}

void QAndroidPlatformIntegration::setScreenOrientation(Qt::ScreenOrientation currentOrientation,
                                                       Qt::ScreenOrientation nativeOrientation)
{
    m_orientation = currentOrientation;
    m_nativeOrientation = nativeOrientation;
}

#ifndef QT_NO_ACCESSIBILITY
QPlatformAccessibility *QAndroidPlatformIntegration::accessibility() const
{
    if (!m_accessibility)
        m_accessibility = new QAndroidPlatformAccessibility();
    return m_accessibility;
}
#endif

void QAndroidPlatformIntegration::setDesktopSize(int width, int height)
{
    if (m_primaryScreen)
        QMetaObject::invokeMethod(m_primaryScreen, "setGeometry", Qt::AutoConnection, Q_ARG(QRect, QRect(0,0,width, height)));
}

void QAndroidPlatformIntegration::setDisplayMetrics(int width, int height)
{
    if (m_primaryScreen)
        QMetaObject::invokeMethod(m_primaryScreen, "setPhysicalSize", Qt::AutoConnection, Q_ARG(QSize, QSize(width, height)));
}

QT_END_NAMESPACE