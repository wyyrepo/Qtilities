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

#include "NamingPolicyFilter.h"
#include "QtilitiesCoreGuiConstants.h"
#include "NamingPolicyInputDialog.h"
#include "QtilitiesCoreGui.h"
#include "QtilitiesPropertyChangeEvent.h"

#include <Observer.h>
#include <QtilitiesCoreConstants.h>
#include <QtilitiesCore.h>
#include <Logger.h>

#include <QMessageBox>
#include <QInputDialog>
#include <QAbstractButton>
#include <QtDebug>
#include <QPushButton>
#include <QMutex>
#include <QVariant>
#include <QRegExpValidator>
#include <QCoreApplication>

using namespace Qtilities::CoreGui::Constants;
using namespace Qtilities::Core::Properties;

// ----------------------------------------------------------------------------------------
// NAMING POLICY SUBJECT FILTER
// ----------------------------------------------------------------------------------------

namespace Qtilities {
    namespace CoreGui {
        FactoryItem<AbstractSubjectFilter, NamingPolicyFilter> NamingPolicyFilter::factory;
    }
}

Qtilities::CoreGui::NamingPolicyFilter::NamingPolicyFilter(QObject* parent) : AbstractSubjectFilter(parent) {
    d_uniqueness_policy = NamingPolicyFilter::ProhibitDuplicateNames;
    d_uniqueness_resolution_policy = NamingPolicyFilter::PromptUser;
    d_validity_resolution_policy = NamingPolicyFilter::PromptUser;
    const QRegExp default_expression(".{1,100}",Qt::CaseInsensitive);
    QRegExpValidator* default_validator = new QRegExpValidator(default_expression,0);
    validator = default_validator;
    name_dialog = new NamingPolicyInputDialog();
    name_dialog->setNamingPolicyFilter(this);

    validation_cycle_active = false;
}

Qtilities::CoreGui::NamingPolicyFilter::~NamingPolicyFilter() {
    if (validator)
        delete validator;

    // Check if this policy filter was the object name manager
    for (int i = 0; i < observer->subjectCount(); i++) {
        assignNewNameManager(observer->subjectAt(i));
    }
}

void Qtilities::CoreGui::NamingPolicyFilter::setUniquenessPolicy(NamingPolicyFilter::UniquenessPolicy uniqueness_policy) {
    // Only change policy if the observer context is not defined for the subject filter.
    if (!observer) {
        d_uniqueness_policy = uniqueness_policy;
    } else {
        if (observer->subjectCount() == 0)
            d_uniqueness_policy = uniqueness_policy;
    }
}

void Qtilities::CoreGui::NamingPolicyFilter::setUniquenessResolutionPolicy(NamingPolicyFilter::ResolutionPolicy uniqueness_resolution_policy) {
    d_uniqueness_resolution_policy = uniqueness_resolution_policy;
}

void Qtilities::CoreGui::NamingPolicyFilter::setValidityResolutionPolicy(NamingPolicyFilter::ResolutionPolicy validity_resolution_policy) {
    // Q_ASSERT(validity_resolution_policy != Replace);
    // Note that replace is not a valid option for invalid names. This resolution will reappear in a future release.

    d_validity_resolution_policy = validity_resolution_policy;
}

Qtilities::CoreGui::NamingPolicyFilter::NameValidity Qtilities::CoreGui::NamingPolicyFilter::evaluateName(QString name) const {
    NamingPolicyFilter::NameValidity result = Acceptable;

    // Check uniqueness of name
    if (observer->subjectNames().contains(name) && d_uniqueness_policy == ProhibitDuplicateNames)
        result |= Duplicate;

    // Validate name using QValidator
    int pos;
    if (validator->validate(name,pos) != QValidator::Acceptable)
        result |= Invalid;

    return result;
}

QObject* Qtilities::CoreGui::NamingPolicyFilter::getConflictingObject(QString name) const {
    if (observer->subjectNames().contains(name) && d_uniqueness_policy == ProhibitDuplicateNames) {
        for (int i = 0; i < observer->subjectCount(); i++) {
            if (observer->subjectNames().at(i) == name)
                return observer->subjectAt(i);
        }
    }

    return 0;
}

Qtilities::CoreGui::AbstractSubjectFilter::EvaluationResult Qtilities::CoreGui::NamingPolicyFilter::evaluateAttachment(QObject* obj) const {
    // Check the validity of obj's name
    NamingPolicyFilter::NameValidity validity_result = evaluateName(obj->objectName());

    if ((validity_result & Invalid) && d_validity_resolution_policy == Reject) {
        return AbstractSubjectFilter::Rejected;
    } else if ((validity_result & Invalid) && d_validity_resolution_policy == PromptUser) {
        return AbstractSubjectFilter::Conditional;
    }

    if ((validity_result & Duplicate) && d_uniqueness_resolution_policy == Reject) {
        return AbstractSubjectFilter::Rejected;
    } else if ((validity_result & Duplicate) && d_uniqueness_resolution_policy == PromptUser) {
        return AbstractSubjectFilter::Conditional;
    }

    return AbstractSubjectFilter::Allowed;
}

bool Qtilities::CoreGui::NamingPolicyFilter::initializeAttachment(QObject* obj) {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(observer != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif

    if (!observer) {
        LOG_TRACE("Cannot evaluate an attachment in a subject filter without an observer context.");
        return false;
    }

    rollback_name = obj->objectName();

    // Get name of new subject/object
    // New names are extracted in the following order
    // 1. obj->property(OBJECT_NAME)
    // 2. If (1) does not exist, we take obj->objectName()
    // This function, as well as the NamingPolicyInputDialog uses the OBJECT_NAME property throughout, and then syncs it with objectName() at the end of the function.
    QString new_name = obj->objectName();;
    bool validation_result = true;
    QVariant name_property = observer->getObserverPropertyValue(obj,OBJECT_NAME);
    if (!name_property.isValid()) {
        // In this case, we create the needed properties and add it to the object.
        // It will be removed if attachment fails anywhere
        SharedObserverProperty new_subject_name_property(QVariant(new_name),OBJECT_NAME);
        new_subject_name_property.setIsExportable(false);
        observer->setSharedProperty(obj,new_subject_name_property);
        SharedObserverProperty object_name_manager_property(QVariant(observer->observerID()),OBJECT_NAME_MANAGER_ID);
        object_name_manager_property.setIsExportable(false);
        observer->setSharedProperty(obj,object_name_manager_property);

        // Check validity of the name
        validation_result = validateNamePropertyChange(obj,OBJECT_NAME);
    } else {
        new_name = name_property.toString();

        // Check if it does not have a name manager yet, in that case we add a name manager
        QVariant name_property = observer->getObserverPropertyValue(obj,OBJECT_NAME_MANAGER_ID);
        if (!name_property.isValid()) {
            SharedObserverProperty object_name_manager_property(QVariant(observer->observerID()),OBJECT_NAME_MANAGER_ID);
            object_name_manager_property.setIsExportable(false);
            observer->setSharedProperty(obj,object_name_manager_property);
        }
    }

    // Check if an instance name must be created.
    // The object manager uses OBJECT_NAME, thus we don't create an instance for it ever, only do it if this observer is not the manager.
    if (!isObjectNameManager(obj)) {
        if (d_uniqueness_policy == ProhibitDuplicateNames) {
            ObserverProperty current_instance_names_property = observer->getObserverProperty(obj,INSTANCE_NAMES);
            if (current_instance_names_property.isValid()) {
                // Thus, the property already exists
                current_instance_names_property.addContext(QVariant(new_name),observer->observerID());
                observer->setObserverProperty(obj,current_instance_names_property);
            } else {
                // We need to create the property and add it to the object
                ObserverProperty new_instance_names_property(INSTANCE_NAMES);
                new_instance_names_property.setIsExportable(false);
                new_instance_names_property.addContext(QVariant(new_name),observer->observerID());
                observer->setObserverProperty(obj,new_instance_names_property);
            }

            // Check validity of the name
            validation_result = validateNamePropertyChange(obj,OBJECT_NAME);
        }
    }

    // Sync objectName() with the OBJECT_NAME property since the event filter is not installed yet.
    // Only do this if this observer is the object name manager
    if (isObjectNameManager(obj)) {
        obj->setObjectName(observer->getObserverPropertyValue(obj,OBJECT_NAME).toString());
        // Do not emit dirty property signal because it is not yet attached to the observer at this stage.
        // Post an QtilitiesPropertyChangeEvent on this object notifying that the name changed.
        QByteArray property_name_byte_array = QByteArray(OBJECT_NAME);
        QtilitiesPropertyChangeEvent* user_event = new QtilitiesPropertyChangeEvent(property_name_byte_array,observer->observerID());
        QCoreApplication::postEvent(obj,user_event);
        LOG_TRACE(QString("Posting QtilitiesPropertyChangeEvent (property: %1) to object (%2)").arg(OBJECT_NAME).arg(obj->objectName()));
    }
    return validation_result;
}

void Qtilities::CoreGui::NamingPolicyFilter::finalizeAttachment(QObject* obj, bool attachment_successful) {
    if (!attachment_successful) {
        // Undo possible name changes that happened in initializeAttachment()
        if (isObjectNameManager(obj))
            observer->setObserverPropertyValue(obj,OBJECT_NAME,QVariant(rollback_name));
        else {
            // First check if the object has a instance names property then
            if (d_uniqueness_policy == ProhibitDuplicateNames)
                observer->setObserverPropertyValue(obj,INSTANCE_NAMES,QVariant(rollback_name));
        }
    }
}

Qtilities::CoreGui::AbstractSubjectFilter::EvaluationResult Qtilities::CoreGui::NamingPolicyFilter::evaluateDetachment(QObject* obj) const {
    return AbstractSubjectFilter::Allowed;
}

void Qtilities::CoreGui::NamingPolicyFilter::finalizeDetachment(QObject* obj, bool detachment_successful, bool subject_deleted) {
    if (detachment_successful && !subject_deleted)
        assignNewNameManager(obj);
}

QStringList Qtilities::CoreGui::NamingPolicyFilter::monitoredProperties() {
    QStringList reserved_properties;
    reserved_properties << QString(OBJECT_NAME) << QString(OBJECT_NAME_MANAGER_ID) << QString(INSTANCE_NAMES);
    return reserved_properties;
}

bool Qtilities::CoreGui::NamingPolicyFilter::monitoredPropertyChanged(QObject* obj, const char* property_name, QDynamicPropertyChangeEvent* propertyChangeEvent) {
    // Use QMutexLocker locker(&filter_mutex) in the future
    if(!filter_mutex.tryLock())
        return true;

    Q_ASSERT(observer != 0);

    // If OBJECT_NAME changed and this observer is the object name manager, we need to react to this change
    if (!strcmp(property_name,OBJECT_NAME)) {
        if (isObjectNameManager(obj)) {
            // Since this observer is the object manager it will make sure that objectName() match the OBJECT_NAME property
            if (!isObjectNameDirty(obj)) {
                // Ok, we know that the property did not change, or its invalid, thus its being added or removed. We never block these action.
                filter_mutex.unlock();
                return false;
            }

            bool return_value = validateNamePropertyChange(obj,OBJECT_NAME);
            if (return_value) {
                QString new_name = observer->getObserverPropertyValue(obj,OBJECT_NAME).toString();
                if (!new_name.isEmpty()) {
                    LOG_DEBUG("Sync'ed objectName() with OBJECT_NAME property -> " + new_name);
                    obj->setObjectName(new_name);
                }
            } else {
                LOG_WARNING(QString(tr("Property change event from objectName() = %1 to OBJECT_NAME property = %2 aborted.")).arg(obj->objectName()).arg(observer->getObserverPropertyValue(obj,OBJECT_NAME).toString()));
            }

            filter_mutex.unlock();
            return (!return_value);
        } else
            filter_mutex.unlock();
            return false;
    } else if (!strcmp(property_name,INSTANCE_NAMES)) {
        ObserverProperty instance_property = observer->getObserverProperty(obj,INSTANCE_NAMES);
        Q_ASSERT(instance_property.isValid());

        if (instance_property.lastChangedContext() == observer->observerID()) {
            bool return_value = validateNamePropertyChange(obj,INSTANCE_NAMES);
            if (return_value) {
                LOG_DEBUG(QString("Detected and handled INSTANCE_NAMES property change to \"%1\" within context \"%2\"").arg(observer->getObserverPropertyValue(obj,OBJECT_NAME).toString()).arg(observer->observerName()));
            } else {
                LOG_WARNING(QString(tr("Aborted INSTANCE_NAMES property change event (attempted change to \"%1\" within context \"%2\").")).arg(observer->getObserverPropertyValue(obj,OBJECT_NAME).toString()).arg(observer->observerName()));
            }

            filter_mutex.unlock();
            return (!return_value);
        }
    } else if (!strcmp(property_name,OBJECT_NAME_MANAGER_ID)) {
        // Use makeNameManager() function to do this.
        return true;
    }

    filter_mutex.unlock();
    return false;
}

bool Qtilities::CoreGui::NamingPolicyFilter::exportFilterSpecificBinary(QDataStream& stream) const {
    stream << rollback_name;
    stream << (quint32) d_uniqueness_policy;
    stream << (quint32) d_uniqueness_resolution_policy;
    stream << (quint32) d_validity_resolution_policy;

    return true;
}

bool Qtilities::CoreGui::NamingPolicyFilter::importFilterSpecificBinary(QDataStream& stream) {
    stream >> rollback_name;

    quint32 ui32;
    stream >> ui32;
    d_uniqueness_policy = (UniquenessPolicy) ui32;
    stream >> ui32;
    d_uniqueness_resolution_policy = (ResolutionPolicy) ui32;
    stream >> ui32;
    d_validity_resolution_policy = (ResolutionPolicy) ui32;

    return true;
}

bool Qtilities::CoreGui::NamingPolicyFilter::validateNamePropertyChange(QObject* obj, const char* property_name) {
    QString changed_name = observer->getObserverPropertyValue(obj,property_name).toString();
    NamingPolicyFilter::NameValidity validity_result = evaluateName(changed_name);
    bool return_value;
    if (changed_name.isEmpty())
        return_value = false;
    else
        return_value = true;

    // Ok invalid names must be handled first
    if (validity_result & Invalid) {
        if (d_validity_resolution_policy == PromptUser) {
            if (validation_cycle_active && name_dialog->useCycleResolution()) {
                if (name_dialog->selectedResolution() == Reject)
                    return_value = false;
                else {
                    name_dialog->setObject(obj);
                    name_dialog->setContext(observer->observerID(),observer->observerName());
                    // The initialize call will recalculate a valid name if needed.
                    name_dialog->initialize(validity_result);
                    // Next we set the name of the object using the name_dialog
                    name_dialog->setName(name_dialog->autoGeneratedName());
                    return_value = true;
                }
            } else {
                name_dialog->setObject(obj);
                name_dialog->setContext(observer->observerID(),observer->observerName());
                name_dialog->initialize(validity_result);
                if (name_dialog->exec()) {
                    if (name_dialog->selectedResolution() == Reject)
                        return_value = false;
                    else
                        return_value = true;
                } else
                    return_value = false;
            }
        } else if (d_validity_resolution_policy == AutoRename) {
            QString valid_name = generateValidName(changed_name);
            if (valid_name.isEmpty())
                return_value = false;
            observer->setObserverPropertyValue(obj,property_name,QVariant(valid_name));
            return_value = true;
        } else if (d_validity_resolution_policy == Reject) {
            return_value = false;
        }
    } else if ((validity_result & Duplicate) && (d_uniqueness_policy == ProhibitDuplicateNames) && (getConflictingObject(changed_name) != obj)) {
        if (d_uniqueness_resolution_policy == PromptUser) {
            name_dialog->setObject(obj);
            name_dialog->setContext(observer->observerID(),observer->observerName());
            name_dialog->initialize(validity_result);
            if (name_dialog->exec()) {
                if (name_dialog->selectedResolution() == Reject)
                    return_value = false;
                else
                    return_value = true;
            } else
                return_value = false;
        } else if (d_uniqueness_resolution_policy == AutoRename) {
            QString valid_name = generateValidName(changed_name);
            if (valid_name.isEmpty())
                return_value = false;
            observer->setObserverPropertyValue(obj,property_name,QVariant(valid_name));
            return_value = true;
        } else if (d_uniqueness_resolution_policy == Reject) {
            return_value = false;
        }
    }
    return return_value;
}

//! Sets the QValidator to be used to determine valid subject names.
/*!
  \note NamingPolicyFilter takes ownership of the new validator after this call.
  */
void Qtilities::CoreGui::NamingPolicyFilter::setValidator(QValidator* valid_naming_validator) {
    if (!valid_naming_validator)
        return;

    if (observer->subjectCount() > 0)
        return;

    validator = valid_naming_validator;
}

//! Gets the QValidator which is used to determine valid subject names.
QValidator* const Qtilities::CoreGui::NamingPolicyFilter::getValidator() {
    return validator;
}

void Qtilities::CoreGui::NamingPolicyFilter::makeNameManager(QObject* obj) {
    // Ok, check if this observer context is observing this object, if not we can't make it a name manager
    ObserverProperty observer_list = observer->getObserverProperty(obj,OBSERVER_SUBJECT_IDS);
    if (observer_list.isValid()) {
        if (!observer_list.hasContext(observer->observerID())) {
            LOG_WARNING(QString(tr("Cannot make observer (%1) the name manager of object (%2). This observer is not currently observing this object.")).arg(observer->observerName()).arg(obj->objectName()));
            return;
        }
    } else {
        LOG_WARNING(QString(tr("Cannot make observer (%1) the name manager of object (%2). This observer is not currently observing this object.")).arg(observer->observerName()).arg(obj->objectName()));
        return;
    }

    // Check if it has a name manager already, if so we add it to the instance names list
    SharedObserverProperty current_manager_id = observer->getSharedProperty(obj,OBJECT_NAME_MANAGER_ID);
    current_manager_id.setIsExportable(false);
    if (current_manager_id.isValid()) {
        if (current_manager_id.value().toInt() == observer->observerID()) {
            LOG_WARNING(QString(tr("Cannot make observer (%1) the name manager of object (%2). This observer is currently the name manager for this object.")).arg(observer->observerName()).arg(obj->objectName()));
            return;
        } else {
            Observer* current_manager = QtilitiesCore::instance()->objectManager()->observerReference(current_manager_id.value().toInt());
            Q_ASSERT(current_manager);
            NamingPolicyFilter* naming_filter = 0;
            for (int i = 0; i < current_manager->subjectFilters().count(); i++) {
                // Check if it is a naming policy subject filter
                naming_filter = qobject_cast<NamingPolicyFilter*> (current_manager->subjectFilters().at(i));
            }

            // Add it to the instance name list only if the current manager has a unique naming policy filter
            if (naming_filter) {
                if (naming_filter->uniquenessNamingPolicy() == ProhibitDuplicateNames) {
                    ObserverProperty current_instance_names_property = observer->getObserverProperty(obj,INSTANCE_NAMES);
                    if (current_instance_names_property.isValid()) {
                        current_instance_names_property.addContext(QVariant(obj->objectName()),current_manager->observerID());
                        observer->setObserverProperty(obj,current_instance_names_property);
                    } else {
                        // We need to create the property and add it to the object
                        ObserverProperty new_instance_names_property(INSTANCE_NAMES);
                        new_instance_names_property.setIsExportable(false);
                        new_instance_names_property.addContext(QVariant(obj->objectName()),observer->observerID());
                        observer->setObserverProperty(obj,new_instance_names_property);
                    }
                }
            }
        }
    }

    // Set this naming policy filter as the new name manager
    QString new_managed_name;

    // If this filter has a unique policy, we need to get the new name from the instance name list and remove this context
    if (d_uniqueness_policy == ProhibitDuplicateNames) {
        ObserverProperty current_instance_names_property = observer->getObserverProperty(obj,INSTANCE_NAMES);
        if (current_instance_names_property.isValid()) {
            new_managed_name = current_instance_names_property.value(observer->observerID()).toString();
            current_instance_names_property.removeContext(observer->observerID());
            observer->setObserverProperty(obj,current_instance_names_property);
        }
        obj->setObjectName(new_managed_name);
        observer->setObserverPropertyValue(obj,OBJECT_NAME,new_managed_name);
    }

    observer->setObserverPropertyValue(obj,OBJECT_NAME_MANAGER_ID,observer->observerID());
    emit notifyDirtyProperty(OBJECT_NAME);
}

void Qtilities::CoreGui::NamingPolicyFilter::startValidationCycle() {
    validation_cycle_active = true;
}

void Qtilities::CoreGui::NamingPolicyFilter::endValidationCycle() {
    validation_cycle_active = false;
    name_dialog->endValidationCycle();
}

bool Qtilities::CoreGui::NamingPolicyFilter::isValidationCycleActive() const {
    return validation_cycle_active;
}

void Qtilities::CoreGui::NamingPolicyFilter::assignNewNameManager(QObject* obj) {
    if (isObjectNameManager(obj)) {
        // Get the next available observer with a naming policy subject filter
        ObserverProperty observer_list = observer->getObserverProperty(obj,OBSERVER_SUBJECT_IDS);
        Observer* next_observer = 0;
        bool found = false;
        if (observer_list.isValid()) {
            for (int i = 0; i < observer_list.observerMap().count(); i++) {
                if (observer_list.observerMap().keys().at(i) != observer->observerID()) {
                    next_observer = QtilitiesCore::instance()->objectManager()->observerReference(observer_list.observerMap().keys().at(i));
                    if (next_observer) {
                        for (int i = 0; i < next_observer->subjectFilters().count(); i++) {
                            // Check if it is a naming policy subject filter
                            NamingPolicyFilter* naming_filter = qobject_cast<NamingPolicyFilter*> (next_observer->subjectFilters().at(i));
                            if (naming_filter) {
                                found = true;
                                // MOD, should make it quicker but crashes at the moment, todo figure out why it happens.
                                // next_observer->setObserverPropertyValue(obj,OBJECT_NAME_MANAGER_ID,-1);
                                naming_filter->makeNameManager(obj);
                                LOG_INFO(QString(tr("The name manager (%1) of object (%2) not observing this object any more. Observer (%3) was selected to be the new name manager for this object.")).arg(observer->observerName()).arg(obj->objectName()).arg(next_observer->observerName()));
                            }
                        }
                    }
                }
            }
        }

        if (!found || !next_observer) {
            // An alternative was not found
            obj->setProperty(INSTANCE_NAMES,QVariant());
            obj->setProperty(OBJECT_NAME_MANAGER_ID,QVariant());
            LOG_WARNING(QString(tr("The name manager (%1) of object (%2) is not observing this object any more. An alternative name manager could not be found. This object's name won't be managed until it is attached to a new observer with a naming policy subject filter.")).arg(observer->observerName()).arg(obj->objectName()));
        }
    }
}

bool Qtilities::CoreGui::NamingPolicyFilter::isObjectNameManager(QObject* obj) const {
    QVariant object_name_manager_variant = observer->getObserverPropertyValue(obj,OBJECT_NAME_MANAGER_ID);
    if (object_name_manager_variant.isValid()) {
        return (object_name_manager_variant.toInt() == observer->observerID());
    } else
        return false;
}

bool Qtilities::CoreGui::NamingPolicyFilter::isObjectNameDirty(QObject* obj) const {
    QString changed_name = observer->getObserverPropertyValue(obj,OBJECT_NAME).toString();
    QVariant observer_property = obj->property(OBJECT_NAME);
    if (changed_name == obj->objectName() || !(observer_property.isValid()))
        return false;
    else
        return true;
}

QString Qtilities::CoreGui::NamingPolicyFilter::generateValidName(QString input_name, bool force_change) {
    if (input_name.isNull())
        input_name = QString("new_object");

    // Check, if it is valid just return it.
    NamingPolicyFilter::NameValidity validity_result = evaluateName(input_name);
    if (validity_result == Acceptable) {
        if (!force_change)
            return input_name;
    }

    // If it's invalid try to fix up the string using QValidator
    if (validity_result & Invalid) {
        validator->fixup(input_name);
        validity_result = evaluateName(input_name);
        if (validity_result == Acceptable) {
            // Ok, fixup fixed it. Send back the fixed value
            return input_name;
        }
    }

    QString new_name;
    if (!(validity_result & Invalid) && ((validity_result & Duplicate) || force_change)) {
        // Fixup made it valid, but a duplicate exists, thus append the value of a counter
        int counter = 0;

        QString section = input_name.section("_",-1);
        bool ok = false;
        bool use_space = false;
        if (section.toInt(&ok))
            counter = section.toInt(&ok);
        else {
            section = input_name.section(" ",-1);
            if (section.toInt(&ok)) {
                counter = section.toInt(&ok);
                use_space = true;
            } else
                section = input_name;
        }

        ++counter;
        if (section.size() != input_name.size()) {
            if (use_space)
                new_name = QString("%1 %2").arg(input_name.left(input_name.size()-section.size()-1)).arg(counter);
            else
                new_name = QString("%1_%2").arg(input_name.left(input_name.size()-section.size()-1)).arg(counter);
        } else
            new_name = QString("%1_%2").arg(input_name).arg(counter);

        while ((evaluateName(new_name) != Acceptable) || (input_name == new_name)) {
            if (use_space)
                section = new_name.section(" ",-1);
            else
                section = new_name.section("_",-1);
            new_name = new_name.left(new_name.size()-section.size()-1);
            ++counter;
            if (use_space)
                new_name = QString("%1 %2").arg(new_name).arg(counter);
            else
                new_name = QString("%1_%2").arg(new_name).arg(counter);
        }
    } else if ((validity_result & Invalid) && !(validity_result & Duplicate)) {
        // Since fixup did not know how to fix it, we will try a few things
        // If you get here, it is probably better to write your own QValidator
        // and provide a proper fixup implementation
        QString experiment_string = input_name;

        // Try 1: Remove whitespaces
        experiment_string.remove(QChar(' '), Qt::CaseInsensitive);
        if (evaluateName(experiment_string) == Acceptable)
            return experiment_string;

        // Try 2: Try a simple string
        experiment_string = QString("new_object");
        if (evaluateName(experiment_string) == Acceptable)
            return experiment_string;

        // Try 3: Try a simple string without an underscore
        experiment_string = QString("NewObject");
        if (evaluateName(experiment_string) == Acceptable)
            return experiment_string;
    }

    return new_name;
}

//------------------------------------------------------------------------
//
// Naming Policy Delegate
//
//------------------------------------------------------------------------

struct Qtilities::CoreGui::NamingPolicyDelegateData {
    QMutex editing_mutex;
    NamingPolicyFilter* naming_filter;
    Observer* observer;
    QString entry_string;
    QObject* obj;
};

Qtilities::CoreGui::NamingPolicyDelegate::NamingPolicyDelegate(QObject *parent) {
    d = new NamingPolicyDelegateData;
    d->observer = 0;
    d->naming_filter = 0;
    d->obj = 0;
}

void Qtilities::CoreGui::NamingPolicyDelegate::setObserverContext(Observer* observer) {
    d->observer = observer;

    if (d->observer) {
        // Look which known subject filters are installed in this observer
        for (int i = 0; i < observer->subjectFilters().count(); i++) {
            // Check if it is a naming policy subject filter
            NamingPolicyFilter* naming_filter = qobject_cast<NamingPolicyFilter*> (observer->subjectFilters().at(i));
            if (naming_filter)
                d->naming_filter = naming_filter;
        }
    }
}

Qtilities::CoreGui::Observer* Qtilities::CoreGui::NamingPolicyDelegate::observerContext() const {
    return d->observer;
}

QWidget *Qtilities::CoreGui::NamingPolicyDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QLineEdit *editor = new QLineEdit(parent);
    connect(editor,SIGNAL(textChanged(QString)),SLOT(on_LineEdit_TextChanged(QString)));

    if (d->observer && d->naming_filter) {
        if (d->naming_filter->getValidator()) {
            editor->setValidator(d->naming_filter->getValidator());
        }
    }

    return editor;
}

void Qtilities::CoreGui::NamingPolicyDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    QString value = index.model()->data(index, Qt::EditRole).toString();

    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
    d->entry_string = value;
}

void Qtilities::CoreGui::NamingPolicyDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
     QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
     QString value = lineEdit->text();

     if (d->observer) {
        LOG_TRACE(QString("Naming control delegate delegated object name within context (%1).").arg(d->observer->observerName()));
     }
     model->setData(index, value, Qt::EditRole);
}

void Qtilities::CoreGui::NamingPolicyDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const {
     editor->setGeometry(option.rect);
}

void Qtilities::CoreGui::NamingPolicyDelegate::on_LineEdit_TextChanged(const QString & text) {
    if (d->observer && d->naming_filter) {
        if (d->naming_filter->uniquenessNamingPolicy() == NamingPolicyFilter::ProhibitDuplicateNames) {
            if (!d->editing_mutex.tryLock())
                return;

            QLineEdit* editor = qobject_cast<QLineEdit*> (sender());
            if (!editor || (text.length() == 0) || (text == d->entry_string)) {
                d->editing_mutex.unlock();
                return;
            }

            editor->setStyleSheet("color: black");
            editor->setToolTip(tr(""));

            NamingPolicyFilter::NameValidity validity_result = d->naming_filter->evaluateName(text);
            if (validity_result != NamingPolicyFilter::Acceptable) {
                if (d->naming_filter->getConflictingObject(text) != d->obj) {
                    editor->setStyleSheet("color: red");
                    if (validity_result == NamingPolicyFilter::Duplicate)
                        editor->setToolTip(tr("The name already exists. Duplicate names are not allowed in this context."));
                    else if (validity_result == NamingPolicyFilter::Invalid)
                        editor->setToolTip(tr("The name contains invalid characters for this context."));
                }
            }

            d->editing_mutex.unlock();
        }
    }
}

void Qtilities::CoreGui::NamingPolicyDelegate::handleCurrentObjectChanged(QList<QObject*> object_list) {
    if (object_list.count() == 1)
        d->obj = object_list.front();
}