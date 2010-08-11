/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#ifndef SESSION_LOG_PLUGIN_CORE_H
#define SESSION_LOG_PLUGIN_CORE_H

#include "SessionLogPlugin_global.h"

#include <IPlugin.h>
#include <QObject>

namespace Qtilities {
    namespace Plugins {
        namespace SessionLog {
            using namespace ExtensionSystem::Interfaces;

            /*!
              \struct SessionLogPluginData
              \brief The SessionLogPluginData struct stores private data used by the SessionLogPlugin class.
             */
            struct SessionLogPluginData;

            /*!
              \class SessionLogPlugin
              \brief A plugin which provides a session log mode to the application.
             */
            class SESSION_LOG_PLUGIN_SHARED_EXPORT SessionLogPlugin : public IPlugin
            {
                Q_OBJECT
                Q_INTERFACES(Qtilities::ExtensionSystem::Interfaces::IPlugin)

            public:
                SessionLogPlugin(QObject* parent = 0);
                ~SessionLogPlugin();

                // --------------------------------------------
                // IPlugin Implementation
                // --------------------------------------------
                bool initialize(const QStringList &arguments, QString *errorString);
                bool initializeDependancies(QString *errorString);
                void finalize();
                double pluginVersion();
                QStringList pluginCompatibilityVersions();
                QString pluginPublisher();
                QString pluginPublisherWebsite();
                QString pluginPublisherContact();
                QString pluginDescription();
                QString pluginCopyright();
                QString pluginLicense();

            private:
                SessionLogPluginData* d;
            };
        }
    }
}

#endif // SESSION_LOG_PLUGIN_CORE_H