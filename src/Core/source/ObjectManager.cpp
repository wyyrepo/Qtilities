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

#include "ObjectManager.h"
#include "QtilitiesCore.h"
#include "ObserverProperty.h"
#include "QtilitiesCoreConstants.h"
#include "Observer.h"
#include "SubjectTypeFilter.h"
#include "AbstractSubjectFilter.h"
#include "Factory.h"
#include "ActivityPolicyFilter.h"
#include "SubjectTypeFilter.h"
#include "ObserverRelationalTable.h"

#include <Logger.h>

#include <QVariant>
#include <QMap>
#include <QPointer>
#include <QtCore>

using namespace Qtilities::Core::Constants;
using namespace Qtilities::Core::Properties;

struct Qtilities::Core::ObjectManagerData {
    ObjectManagerData() : object_pool(GLOBAL_OBJECT_POOL,"Pool of exposed global objects."),
    id(1)  { }

    QMap<int,QPointer<Observer> >       observer_map;
    QMap<QString, IFactory*>            factory_map;
    QMap<QString, QList<QObject*> >     meta_type_map;
    Observer                            object_pool;
    int                                 id;
    Factory<AbstractSubjectFilter>      subject_filter_factory;
};

Qtilities::Core::ObjectManager::ObjectManager(QObject* parent) : IObjectManager(parent)
{
    d = new ObjectManagerData;
    d->object_pool.startProcessingCycle();

    Observer::ActionHints action_hints = 0;
    action_hints |= Observer::RefreshView;
    action_hints |= Observer::PushDown;
    action_hints |= Observer::PushDownNew;
    action_hints |= Observer::PushUp;
    action_hints |= Observer::PushUpNew;
    action_hints |= Observer::SwitchView;
    d->object_pool.setActionHints(action_hints);

    setObjectName("Object Manager");

    // Give the manager an icon TRACK
    /*SharedObserverProperty shared_icon_property(QVariant(QIcon(QString(ICON_MANAGER_16x16))),OBJECT_ICON);
    Q_ASSERT(shared_icon_property.isValid());
    QVariant icon_property = qVariantFromValue(shared_icon_property);
    setProperty(shared_icon_property.propertyName(),icon_property);*/

    // Add the standard observer subject filters which comes with the Qtilities library here
    //FactoryInterfaceData naming_policy_filter(FACTORY_TAG_NAMING_POLICY_FILTER);
    //d->subject_filter_factory.registerFactoryInterface(&NamingPolicyFilter::factory,naming_policy_filter);
    FactoryInterfaceData activity_policy_filter(FACTORY_TAG_ACTIVITY_POLICY_FILTER);
    d->subject_filter_factory.registerFactoryInterface(&ActivityPolicyFilter::factory,activity_policy_filter);
    FactoryInterfaceData subject_type_filter(FACTORY_TAG_SUBJECT_TYPE_FILTER);
    d->subject_filter_factory.registerFactoryInterface(&SubjectTypeFilter::factory,subject_type_filter);
}

Qtilities::Core::ObjectManager::~ObjectManager()
{
    delete d;
}

int Qtilities::Core::ObjectManager::registerObserver(Observer* observer) {
    if (observer) {
        QPointer<Observer> q_pointer = observer;
        d->observer_map[d->id] = q_pointer;
        d->id = d->id + 1;
        return d->id-1;
    }

    return -1;
}

bool Qtilities::Core::ObjectManager::isSupportedType(const QString& meta_type, Observer* observer) {
    if (!observer)
        return false;

    // Check if this observer has a subject type filter installed
    for (int i = 0; i < observer->subjectFilters().count(); i++) {
        SubjectTypeFilter* subject_type_filter = qobject_cast<SubjectTypeFilter*> (observer->subjectFilters().at(i));
        if (subject_type_filter) {
            if (subject_type_filter->isKnownType(meta_type)) {
                return true;
            }
            break;
        }
    }

    return false;
}

Qtilities::Core::Observer* Qtilities::Core::ObjectManager::observerReference(int id) const {
    if (d->observer_map.contains(id)) {
        return d->observer_map.value(id);
    } else if (id == 0) {
        return &d->object_pool;
    } else
        return 0;
}

bool Qtilities::Core::ObjectManager::moveSubjects(QList<QObject*> objects, int source_observer_id, int destination_observer_id) {
    // Get observer references
    Observer* source_observer = observerReference(source_observer_id);
    Observer* destination_observer = observerReference(destination_observer_id);

    if (!source_observer || !destination_observer)
        return false;

    bool none_failed = true;

    // For now we discard objects that cause problems during attachment and detachment
    for (int i = 0; i < objects.count(); i++) {
        // Check if the destination observer will accept it
        Observer::EvaluationResult result = destination_observer->canAttach(objects.at(i));
        if (result == Observer::Rejected) {           
            break;
        } else {
            // Detach from source
            result = source_observer->canDetach(objects.at(i));
            if (result == Observer::Rejected) {
                LOG_ERROR(QString(QObject::tr("The move operation could not be completed. The object you are trying to move was rejected by the destination observer. Check the session log for more details.")));
                none_failed = false;
                break;
            } else if (result == Observer::IsParentObserver) {
                LOG_ERROR(QString(QObject::tr("The move operation could not be completed. The object you are trying to move cannot be removed from the source observer which is defined to be its owner.\n\nTry to share this object with the destination observer instead.")));
                none_failed = false;
                break;
            } else if (result == Observer::LastScopedObserver) {
                destination_observer->setObserverPropertyValue(objects.at(i),OWNERSHIP,QVariant(Observer::ManualOwnership));
                if (!source_observer->detachSubject(objects.at(i))) {
                    destination_observer->setObserverPropertyValue(objects.at(i),OWNERSHIP,QVariant(Observer::ObserverScopeOwnership));
                    none_failed = false;
                    break;
                } else {
                    if (!destination_observer->attachSubject(objects.at(i),Observer::ObserverScopeOwnership))
                        source_observer->attachSubject(objects.at(i),Observer::ObserverScopeOwnership);
                }
            } else {
                if (!source_observer->detachSubject(objects.at(i))) {
                    none_failed = false;
                    break;
                } else {
                    if (!destination_observer->attachSubject(objects.at(i)))
                        source_observer->attachSubject(objects.at(i));
                }
            }
        }
    }

    return none_failed;
}

void Qtilities::Core::ObjectManager::registerObject(QObject* obj) {
    if (d->object_pool.attachSubject(obj))
        emit newObjectAdded(obj);
}

void Qtilities::Core::ObjectManager::registerIFactory(IFactory* factory_iface) {
    if (!factory_iface)
        return;

    foreach(QString tag, factory_iface->factoryTags()) {
        if (!d->factory_map.keys().contains(tag)) {
            d->factory_map[tag] = factory_iface;
        }
    }
}

Qtilities::Core::Interfaces::IFactory* Qtilities::Core::ObjectManager::factoryReference(const QString& tag) const {
    if (d->factory_map.contains(tag))
        return d->factory_map[tag];
    else
        return 0;
}

QList<QObject*> Qtilities::Core::ObjectManager::registeredInterfaces(const QString& iface) const {
    return d->object_pool.subjectReferences(iface);
}

void Qtilities::Core::ObjectManager::setMetaTypeActiveObjects(const QString& subject_type, QList<QObject*> objects, const QStringList& filter_list, bool inversed_list) {
    d->meta_type_map[subject_type] = objects;
    emit metaTypeActiveObjectsChanged(subject_type,objects,filter_list,inversed_list);
}

QList<QObject*> Qtilities::Core::ObjectManager::metaTypeActiveObjects(const QString& subject_type) const {
    return d->meta_type_map[subject_type];
}

quint32 MARKER_OBJECT_PROPERTY_SECTION = 0xCCCCCCCC;

bool Qtilities::Core::ObjectManager::exportObjectProperties(QObject* obj, QDataStream& stream, PropertyTypeFlags property_types) const {
    Q_ASSERT(obj);

    stream << MARKER_OBJECT_PROPERTY_SECTION;
    // First count the number of properties to be exported
    quint32 observer_property_count = 0;
    quint32 shared_property_count = 0;
    QList<ObserverProperty> observer_property_list;
    QList<SharedObserverProperty> shared_property_list;
    // Loop through all dynamic properties and check their exportability.
    for (int p = 0; p < obj->dynamicPropertyNames().count(); p++) {
        ObserverProperty observer_property = Observer::getObserverProperty(obj,obj->dynamicPropertyNames().at(p));
        if (observer_property.isValid() && observer_property.isExportable()) {
            ++observer_property_count;
            observer_property_list << observer_property;
        } else {
            SharedObserverProperty shared_property = Observer::getSharedProperty(obj,obj->dynamicPropertyNames().at(p));
            if (shared_property.isValid() && shared_property.isExportable()) {
                ++shared_property_count;
                shared_property_list << shared_property;
            }
        }
    }

    // Write the shared properties count.
    bool found_visitor_id = false;
    if (property_types & IObjectManager::SharedProperties) {
        stream << shared_property_count;
        LOG_TRACE(QString(tr("Streaming %1 shared properties.")).arg(shared_property_count));
        // Now stream the shared properties.
        for (quint32 p = 0; p < shared_property_count; p++) {
            LOG_TRACE(QString(tr("Streaming shared property: \"%1\"")).arg(QString(shared_property_list.at(p).propertyName())));
            SharedObserverProperty tmp_prop  = shared_property_list.at(p);
            tmp_prop.exportSharedPropertyBinary(stream);
            QString s1 = tmp_prop.propertyName();
            QString s2 = OBSERVER_VISITOR_ID;
            if (s1 == s2)
                found_visitor_id = true;
        }
    } else
        stream << (quint32) 0;

    if (!found_visitor_id)
        LOG_WARNING(QString(tr("No visitor ID property found on object %1")).arg(obj->objectName()));

    stream << MARKER_OBJECT_PROPERTY_SECTION;

    // Write the observer properties count.
    if (property_types & IObjectManager::ObserverProperties) {
        stream << observer_property_count;
        LOG_TRACE(QString(tr("Streaming %1 observer properties.")).arg(observer_property_count));
        // Now stream the observer properties
        for (quint32 p = 0; p < observer_property_count; p++) {
            LOG_TRACE(QString(tr("Streaming observer property: \"%1\"")).arg(QString(observer_property_list.at(p).propertyName())));
            ObserverProperty tmp_prop  = observer_property_list.at(p);
            tmp_prop.exportObserverPropertyBinary(stream);
        }
    } else
        stream << (quint32) 0;

    stream << MARKER_OBJECT_PROPERTY_SECTION;
    return true;
}

bool Qtilities::Core::ObjectManager::importObjectProperties(QObject* new_instance, QDataStream& stream) const {
    Q_ASSERT(new_instance);

    quint32 ui32;
    stream >> ui32;
    if (ui32 != MARKER_OBJECT_PROPERTY_SECTION) {
        LOG_ERROR("ObjectManager::importObjectProperties binary import failed to detect start marker. Import will fail.");
        return false;
    }
    // Now read the properties
    quint32 shared_property_count;
    stream >> shared_property_count;
    bool found_visitor_id = false;
    LOG_TRACE(QString(tr("Streaming %1 shared properties.")).arg(shared_property_count));
    for (int p = 0; p < (int) shared_property_count; p++) {
        SharedObserverProperty shared_property;
        if (!shared_property.importSharedPropertyBinary(stream))
            return false;
        if (shared_property.isValid()) {
            QVariant property = qVariantFromValue(shared_property);
            new_instance->setProperty(shared_property.propertyName(),property);
            QString s1 = shared_property.propertyName();
            QString s2 = OBSERVER_VISITOR_ID;
            if (s1 == s2)
                found_visitor_id = true;
        } else {
            LOG_ERROR("ObjectManager::importObjectProperties binary import detected an invalid shared property. Import will fail.");
            return false;
        }
    }
    if (!found_visitor_id)
        LOG_WARNING(QString(tr("No visitor ID property found on object %1")).arg(new_instance->objectName()));

    stream >> ui32;
    if (ui32 != MARKER_OBJECT_PROPERTY_SECTION) {
        LOG_ERROR("ObjectManager::importObjectProperties binary import failed to detect middle marker. Import will fail.");
        return false;
    }
    quint32 observer_property_count;
    stream >> observer_property_count;
    LOG_TRACE(QString(tr("Streaming %1 observer properties.")).arg(observer_property_count));

    for (int p = 0; p < (int) observer_property_count; p++) {
        ObserverProperty observer_property;
        if (!observer_property.importObserverPropertyBinary(stream))
            return false;
        if (observer_property.isValid()) {
            // Create a new property with the observer property.
            // At this stage, the session IDs will be wrong when importing in a different application session.
            // This is fixed in the relational observer table's constructRelationships() function.
            QVariant property = qVariantFromValue(observer_property);
            new_instance->setProperty(observer_property.propertyName(),property);
        } else {
            LOG_ERROR("ObjectManager::importObjectProperties binary import detected invalid observer property. Import will fail.");
            return false;
        }
    }
    stream >> ui32;
    if (ui32 != MARKER_OBJECT_PROPERTY_SECTION) {
        LOG_ERROR("ObjectManager::importObjectProperties binary import failed to detect end marker. Import will fail.");
        return false;
    }
    return true;
}

bool Qtilities::Core::ObjectManager::constructRelationships(QList<QPointer<QObject> >& objects, ObserverRelationalTable& table) const {
    LOG_TRACE(QString(tr("Starting observer relationship construction on %1 object(s).")).arg(objects.count()));

    // First check if all the objects in the pointer list are present in the table.
    if (!table.compareObjects(objects)) {
        LOG_ERROR(QString(tr("Relational table comparison failed. Relationship construction aborted.")));
        return false;
    } else
        LOG_TRACE("Table comparison successfull.");

    QList<Observer*> observer_list = observerList(objects);
    // Disable subject event filtering on all observers in list:
    for (int i = 0; i < observer_list.count(); i++) {
        observer_list.at(i)->disableSubjectEventFiltering();
    }

    // Fill in all the session ID fields with the current session information
    // and populate the previous session ID filed.
    LOG_TRACE("Populating current session ID fields.");
    for (int i = 0; i < objects.count(); i++) {
        int visitor_id = ObserverRelationalTable::getVisitorID(objects.at(i));
        RelationalTableEntry* entry = table.entryWithVisitorID(visitor_id);
        Observer* obs = qobject_cast<Observer*> (objects.at(i));
        if (!obs) {
            // Check children of the object.
            foreach (QObject* child, objects.at(i)->children()) {
                obs = qobject_cast<Observer*> (child);
                if (obs)
                    break;
            }
        }

        if (obs) {
            entry->d_previousSessionID = entry->d_sessionID;
            entry->d_sessionID = obs->observerID();
        } else {
            entry->d_previousSessionID = -1;
            entry->d_sessionID = -1;
        }
    }

    // Correct the session IDs of all observer properties.
    // Binary exports of observer properties (not shared) stream the complete observer map of the property.
    // Here we need to correct the observer IDs (session IDs) for the current session and remove
    // contexts which was not part of the export.
    LOG_TRACE("Correcting observer property observer ID fields.");
    for (int i = 0; i < objects.count(); i++) {
        // Loop through all dynamic properties and get all the exportable observer properties.
        int observer_property_count = 0;
        QList<ObserverProperty> observer_property_list;
        QObject* obj = objects.at(i);
        for (int p = 0; p < obj->dynamicPropertyNames().count(); p++) {
            ObserverProperty observer_property = Observer::getObserverProperty(obj,obj->dynamicPropertyNames().at(p));
            // Only exportable properties must be modified here:
            if (observer_property.isValid() && observer_property.isExportable()) {
                ++observer_property_count;
                observer_property_list << observer_property;
            }
        }

        // We need to map each observer ID in the observer map to the current session ID for that observer.
        // We do this using the following steps:
        // -) Loop through all properties.
        // -) For each property, get each observer ID (previous session ID) in the observer map.
        // -) Find the object with the previous session ID in the table.
        // -) Gets its current observer ID by casting it to an observer.
        // -) Create a new property with current observer ID and values from current property.
        // -) Lastly replace the property with the new property.
        for (int p = 0; p < observer_property_count; p++) {
            ObserverProperty current_property = observer_property_list.at(p);
            ObserverProperty new_property(current_property.propertyName());
            QMap<int, QVariant> local_map = current_property.observerMap();
            for (int m = 0; m < local_map.count(); m++) {
                int prev_session_id = local_map.keys().at(m);
                RelationalTableEntry* entry = table.entryWithPreviousSessionID(prev_session_id);
                if (!entry) {
                    LOG_ERROR(QString(QObject::tr("ObjectManager::constructRelationships() failed during observer property reconstruction. Failed to find relational table entry for previous session id: %1")).arg(prev_session_id));
                    return false;
                }

                int current_session_id = entry->d_sessionID;
                new_property.addContext(current_property.value(prev_session_id),current_session_id);
            }
            obj->setProperty(current_property.propertyName(),QVariant());
            Observer::setObserverProperty(obj,new_property);
        }
    }

    // Now construct the relationships.
    // We do this by taking the following steps:
    // 1. Go through the list and attach each item to all of its parents.
    //    If it is already attached to a parent, the attachment will just fail.
    //    We get the parent by looking it up in the object list. The lookup is performed by
    //    getting the visitor ID on each object in the list until we find a match.
    //    While going through the list we fill in the sessionID fields of each entry in the relational table.
    //    The sessionID is used again in step 3.
    // 2. Once the parents are sorted out, we need to sort out the object ownership.
    //    This is done by simply setting the OBSERVER_OWNERSHIP property on the object.
    //    If the ownership is SpecificObserverOwnership, we need to set the OBSERVER_PARENT property
    //    as well.
    // 3. Correct the names of each object in all the contexts to which it is attached.
    bool success = true;

    LOG_TRACE("Processing objects in construction list:");
    for (int i = 0; i < objects.count(); i++) {
        // Get the object Visitor ID property:
        int visitor_id = ObserverRelationalTable::getVisitorID(objects.at(i));
        LOG_TRACE(QString(tr("Busy with object %1/%2: %3")).arg(i+1).arg(objects.count()).arg(objects.at(i)->objectName()));

        // Now get this entry in the table:
        RelationalTableEntry* entry = table.entryWithVisitorID(visitor_id);
        if (!entry) {
            LOG_ERROR(tr("Observer relationship construction failed on object: ") + objects.at(i)->objectName() + tr(". An attempt will be made to continue with the rest of the relational table."));
            break;
        }

        // Now attach this subject to each parent using ManualOwnership:
        LOG_TRACE("> Attaching object to all needed contexts.");
        for (int e = 0; e < entry->d_parents.count(); e++) {
            // First get the actual session id (observer ID) for the parent:
            int session_id = table.entryWithVisitorID(entry->d_parents.at(e))->d_sessionID;
            Observer* obs = observerReference(session_id);
            if (obs) {
                // If it was already attached we skip this step.
                if (!obs->contains(objects.at(i))) {
                    obs->attachSubject(objects.at(i));
                    LOG_TRACE(">> Attaching object to context: " + obs->observerName());
                } else {
                    LOG_TRACE(">> Object already attached to context: " + obs->observerName());
                }
            } else {
                LOG_ERROR(QString(tr("Observer ID \"%1\" invalid on object: ")).arg(entry->d_parents.at(e)) + objects.at(i)->objectName() + tr(". An attempt will be made to continue with the rest of the relational table."));
                success = false;
            }
        }

        // Now set the ownership property on the object:
        LOG_TRACE("> Restoring correct ownership for object.");
        if ((Observer::ObjectOwnership) entry->d_ownership == Observer::ManualOwnership) {
            SharedObserverProperty ownership_property(QVariant(Observer::ManualOwnership),OWNERSHIP);
            ownership_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),ownership_property);
            SharedObserverProperty observer_parent_property(QVariant(-1),OBSERVER_PARENT);
            observer_parent_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),observer_parent_property);
            LOG_TRACE(">> Restored object ownership is ManualOwnership.");
        } else if ((Observer::ObjectOwnership) entry->d_ownership == Observer::ObserverScopeOwnership) {
            SharedObserverProperty ownership_property(QVariant(Observer::ObserverScopeOwnership),OWNERSHIP);
            ownership_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),ownership_property);
            SharedObserverProperty observer_parent_property(QVariant(-1),OBSERVER_PARENT);
            observer_parent_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),observer_parent_property);
            LOG_TRACE(">> Restored object ownership is ObserverScopeOwnership.");
        } else if ((Observer::ObjectOwnership) entry->d_ownership == Observer::SpecificObserverOwnership) {
            // Get the session ID of the parent observer:
            RelationalTableEntry* parent_entry = table.entryWithVisitorID(entry->d_parentVisitorID);
            if (parent_entry) {
                int session_id = parent_entry->d_sessionID;
                SharedObserverProperty ownership_property(QVariant(Observer::SpecificObserverOwnership),OWNERSHIP);
                ownership_property.setIsExportable(false);
                Observer::setSharedProperty(objects.at(i),ownership_property);
                SharedObserverProperty observer_parent_property(QVariant(session_id),OBSERVER_PARENT);
                observer_parent_property.setIsExportable(false);
                Observer::setSharedProperty(objects.at(i),observer_parent_property);
                LOG_TRACE(">> Restored object ownership is SpecificObserverOwnership. Owner context ID: " + QString("%1").arg(session_id));
            } else {
                // This will happen when the object is the top level observer which was exported. In this
                // case we need to check if the object has any parents in the observer relational table entry.
                // If so we flag it as an error, else we know that it is not a problem:
                if (entry->d_parents.count() > 0) {
                    LOG_ERROR(QString(QObject::tr("Could not find parent with visitor ID (%1) to which object (%2) must be attached with SpecificObserverOwnership.")).arg(entry->d_parentVisitorID).arg(objects.at(i)->objectName()));
                    success = false;
                }
            }
        } else if ((Observer::ObjectOwnership) entry->d_ownership == Observer::OwnedBySubjectOwnership) {
            SharedObserverProperty ownership_property(QVariant(Observer::OwnedBySubjectOwnership),OWNERSHIP);
            ownership_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),ownership_property);
            SharedObserverProperty observer_parent_property(QVariant(-1),OBSERVER_PARENT);
            observer_parent_property.setIsExportable(false);
            Observer::setSharedProperty(objects.at(i),observer_parent_property);
            LOG_TRACE(">> Restored object ownership is OwnedBySubjectOwnership");
        } else {
            if (entry->d_parents.count() > 0)
                LOG_WARNING(QString(QObject::tr("Could not determine correct ownership for object: %1")).arg(objects.at(i)->objectName()));
        }

        LOG_TRACE("> Restoring instance names accros contexts.");
    }

    // Enable subject event filtering on all observers in the objects list,
    // and emit the modificationStateChanged(true) signal on all the observers.
    // Disable subject event filtering on all observers in list:
    for (int i = 0; i < observer_list.count(); i++) {
        observer_list.at(i)->enableSubjectEventFiltering();
        observer_list.at(i)->refreshViews();
    }

    return success;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::ObjectManager::exportObserverBinary(QDataStream& stream, Observer* obs, bool verbose_output) const {
    if (!obs)
        return IExportable::Failed;

    if (!(obs->supportedFormats() & IExportable::Binary))
        return IExportable::Failed;

    // Export relational data about the observer:
    ObserverRelationalTable table(obs,true);
    if (verbose_output)
        table.dumpTableInfo();

    // Stream the table to a file, and read it back. Then compare it to verify the streaming:
    QTemporaryFile test_file;
    test_file.open();
    QDataStream test_stream_out(&test_file);
    table.exportBinary(test_stream_out);
    test_file.close();
    test_file.open();
    QDataStream test_stream_in(&test_file);    // read the data serialized from the file
    ObserverRelationalTable readback_table;
    readback_table.importBinary(test_stream_in);
    if (verbose_output)
        readback_table.dumpTableInfo();
    test_file.close();
    if (!table.compare(readback_table)) {
        LOG_ERROR(QString(tr("Observer relational table comparison failed. Observer (%1) will not be exported.").arg(obs->observerName())));
        return IExportable::Failed;
    }

    LOG_DEBUG("Exporting observer relational table for design: " + obs->observerName());
    if (verbose_output)
        table.dumpTableInfo();
    table.exportBinary(stream);

    // Now export the observer itself:
    IExportable::Result result = obs->exportBinary(stream);
    QtilitiesCore::instance()->objectManager()->exportObjectProperties(obs,stream);

    // Check result
    if (result == IExportable::Complete) {
        obs->setModificationState(false,true,true);
    } else if (result == IExportable::Incomplete) {
        LOG_WARNING(tr("Observer (") + obs->objectName() + tr(") was only partially saved. Saved project will be incomplete."));
        obs->setModificationState(false,true,true);
    }

    return result;
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::ObjectManager::importObserverBinary(QDataStream& stream, Observer* obs, bool verbose_output) {
    if (!obs)
        return IExportable::Failed;

    if (!(obs->supportedFormats() & IExportable::Binary))
        return IExportable::Failed;

    // First stream the relational table
    ObserverRelationalTable readback_table;
    if (!readback_table.importBinary(stream))
        return IExportable::Failed;
    if (verbose_output)
        readback_table.dumpTableInfo();

    // We must stream the factory data for the observer first.
    QList<QPointer<QObject> > internal_import_list;
    IFactoryData factoryData;
    if (!factoryData.importBinary(stream))
        return IExportable::Failed;

    IExportable::Result result = obs->importBinary(stream, internal_import_list);
    if (result == IExportable::Failed)
        return result;
    QtilitiesCore::instance()->objectManager()->importObjectProperties(obs,stream);
    internal_import_list.append(obs);

    // Construct relationships:
    if (!QtilitiesCore::instance()->objectManager()->constructRelationships(internal_import_list,readback_table))
        result = IExportable::Incomplete;    

    // Cross-check the constructed table:
    ObserverRelationalTable constructed_table(obs,true);
    if (verbose_output)
        constructed_table.dumpTableInfo();
    if (!constructed_table.compare(readback_table)) {
        LOG_WARNING(QString(tr("Relational verification failed on observer: %1")).arg(obs->observerName()));
        result = IExportable::Incomplete;
    } else {
        LOG_INFO(QString(tr("Relational verification successful on observer: %1")).arg(obs->observerName()));
    }

    // Remove all relational properties used.
    ObserverRelationalTable::removeRelationalProperties(obs);

    // Once everything is done we look at result to see if it was succesfull.
    if (result == IExportable::Incomplete) {
        LOG_WARNING(tr("Design files for design (") + obs->observerName() + tr(") was partially reconstructed. Loaded set of design files will be incomplete."));
    } else if (result == IExportable::Failed) {
        // Handle deletion of internal_import_list;
        // Delete the first item in the list (the top item) and the rest should be deleted.
        // For the subjects with manual ownership we delete the remaining items in the list manually.
        while (internal_import_list.count() > 0) {
            if (internal_import_list.at(0) != 0) {
                delete internal_import_list.at(0);
                internal_import_list.removeAt(0);
            } else{
                internal_import_list.removeAt(0);
            }
        }
    }
    return result;
}

QList<Qtilities::Core::Observer*> Qtilities::Core::ObjectManager::observerList(QList<QPointer<QObject> >& object_list) const {
    QList<Observer*> observer_list;
    for (int i = 0; i < object_list.count(); i++) {
        Observer* obs = qobject_cast<Observer*> (object_list.at(i));
        if (!obs) {
            // Check children of the object.
            foreach (QObject* child, object_list.at(i)->children()) {
                obs = qobject_cast<Observer*> (child);
                if (obs)
                    break;
            }
        }
        if (obs)
            observer_list << obs;
    }
    return observer_list;
}

QDataStream &operator<<(QDataStream &ds, Qtilities::Core::SubjectTypeInfo &s) {
    ds << s.d_meta_type;
    ds << s.d_name;
    return(ds);
}

QDataStream &operator>>(QDataStream &ds, Qtilities::Core::SubjectTypeInfo &s) {
    ds >> s.d_meta_type;
    ds >> s.d_name;
    return(ds);
}